# Proto Gateway (ZeroMQ) — 交易系统网关框架

基于 **C++17 / Protobuf / ZeroMQ** 的抽象网关框架，支持通过实例化创建不同业务网关。当前提供 **TradeGateway**（交易通道）和 **QuoteGateway**（行情通道）两个实例。

## 架构设计

```
┌─────────────────────────────────────────────────────────────────┐
│                      framework/ 框架层                           │
│  ┌──────────────┐  ┌──────────┐  ┌──────────────────┐          │
│  │ RouterServer │  │ PubServer│  │ GatewayApi    │          │
│  │ (ROUTER模式) │  │ (PUB模式)│  │ (DEALER+SUB模式) │          │
│  └──────┬───────┘  └────┬─────┘  └────────┬─────────┘          │
├─────────┼───────────────┼─────────────────┼────────────────────┤
│         │               │                 │                     │
│  ┌──────┴───────┐ ┌──────┴────────┐ ┌─────┴──────────┐        │
│  │ trade_gateway  │ │ quote_gateway   │ │ TradeApi       │        │
│  │ (ROUTER)     │ │ (PUB)         │ │ QuoteApi       │        │
│  │ :12345       │ │ :12346        │ │ (DEALER / SUB) │        │
│  └──────────────┘ └───────────────┘ └────────────────┘        │
│       trade/           quote/              SDK 层              │
├────────────────────────────────────────────────────────────────┤
│  业务层：TradeHandler / TradeSpi / QuoteSpi                     │
└────────────────────────────────────────────────────────────────┘
```

### 设计原则

- **框架抽象** — `framework/` 提供 ROUTER/PUB/CLIENT 三种基础模式，不绑定具体业务
- **实例化网关** — `trade/` 和 `quote/` 分别继承框架基类，使用独立 proto、端口、消息类型枚举
- **双通道设计** — TradeGateway 纯请求-应答（ROUTER），QuoteGateway 主题广播（PUB/SUB，客户端本地过滤）
- **接口驱动** — 服务端通过 `TradeHandler`/`QuoteHandler` 注入业务，客户端通过 `TradeSpi`/`QuoteSpi` 接收回调

## 项目结构

```
gateway/
├── framework/                    # 可复用网关框架
│   ├── RouterServer.h/.cpp       # ROUTER 模式服务端基类
│   ├── PubServer.h/.cpp          # PUB 模式发布端基类
│   └── GatewayApi.h/.cpp      # DEALER+SUB 客户端基类
├── trade/                        # 交易网关
│   ├── trade.proto               # 交易协议定义
│   ├── TradeTypes.h              # 类型+消息枚举+错误码
│   ├── TradeHandler.h            # 服务端业务处理器接口
│   ├── trade_gateway.h/.cpp        # 交易服务端
│   └── TradeApi.h/.cpp           # 交易客户端 SDK
├── quote/                        # 行情网关
│   ├── quote.proto               # 行情协议定义
│   ├── QuoteTypes.h              # 类型+消息枚举+错误码
│   ├── quote_gateway.h/.cpp        # 行情服务端（PUB 广播）
│   └── QuoteApi.h/.cpp           # 行情客户端 SDK（SUB 接收）
├── common/                       # 共享
│   ├── message_utils.h/.cpp      # ZMQ 消息收发
│   ├── gateway_msg_types.h       # 基础消息类型
│   └── ThreadPool.h/.cpp         # 线程池
├── server/server.cpp             # 服务端入口
├── client/client.cpp             # 客户端 Demo
├── CMakeLists.txt
└── output/                       # 编译产物
```

## TradeGateway — 交易通道

**端口**: ROUTER on `12345`

**模式**: 请求-应答（ZMQ ROUTER/DEALER）

**消息类型**:

| 枚举 | 值 | 说明 |
|------|---|------|
| `kLoginRequest` | 1 | 登录请求 |
| `kLoginResponse` | 2 | 登录响应 |
| `kPing` | 3 | 心跳请求 |
| `kPong` | 4 | 心跳响应 |
| `kPlaceOrderRequest` | 5 | 下单请求 |
| `kPlaceOrderResponse` | 6 | 下单响应 |
| `kQueryOrderRequest` | 7 | 查询订单请求 |
| `kQueryOrderResponse` | 8 | 查询订单响应 |
| `kCancelOrderRequest` | 9 | 撤单请求 |
| `kCancelOrderResponse` | 10 | 撤单响应 |

**数据流**:
```
Client (TradeApi)                  Server (trade_gateway)
  |                                       |
  |--[DEALER]--LoginRequest-->[ROUTER:12345]
  |                                       |→ TradeHandler::OnLogin
  |<--[ROUTER]--LoginResponse--[DEALER]   |
```

## QuoteGateway — 行情通道

**端口**: PUB on `12346`

**模式**: 纯发布-订阅（ZMQ PUB/SUB），客户端通过本地 `zmq_setsockopt(ZMQ_SUBSCRIBE)` 过滤主题

**消息类型**:

| 枚举 | 值 | 说明 |
|------|---|------|
| `kMarketData` | 1 | 行情数据（PUB） |
| `kNotice` | 2 | 通知广播（PUB） |

**主题格式**:
- `notice` — 系统通知（所有客户端可订阅）
- `market.{SYMBOL}` — 行情数据，如 `market.BTC-USDT`

**ZMQ 订阅过滤**: 客户端的 SUB socket 调用 `Subscribe("notice")` 则只收到通知，`Subscribe("market.")` 则收到所有行情数据，`Subscribe("")` 则收到全部。过滤在客户端本地完成，无需服务端感知订阅状态。

**数据流**:
```
Client (QuoteApi)              Server (quote_gateway)
  |                                   |
  |--[SUB]--Subscribe("notice")------>| (本地 ZMQ 过滤，不发网络请求)
  |                                   |
  |                        PublishNotice("hello")
  |                                   |→ [PUB:12346]
  |<--[PUB]--topic="notice"[Notice]---|
  |→ QuoteSpi::OnNotice               |
```

### Slow-joiner 缓解

PUB/SUB 存在 slow-joiner 问题（新 SUB 连接后 ~100ms 内消息可能丢失）。框架在 `PubServer::Publish()` 中对每条消息重发 3 次（间隔 100ms），并使用 `message_id` 实现客户端去重。

## 构建 & 运行

### 依赖

- CMake >= 3.12
- Protocol Buffers（`brew install protobuf`）
- ZeroMQ 4.x（`brew install zeromq`）
- C++17

### 构建

```bash
cmake -B cmake-build-debug -S .
cmake --build cmake-build-debug
```

### 运行测试

终端 1 — 服务端：
```bash
./output/server
# [RouterServer] listening on tcp://0.0.0.0:12345
# [PubServer] listening on tcp://0.0.0.0:12346
# ========== Gateway Server Menu ==========
#   Trade :0 clients on :12345
#   Quote : PUB on :12346
# ------------------------------------------
# 1. Publish notice to all clients
# 2. Publish simulated market data
# 3. Shutdown server
```

终端 2 — 客户端：
```bash
./output/client
# ========== Gateway Client Demo ==========
# Trade: Disconnected | Quote: Disconnected
# ------------------------------------------
# 1. Connect TradeGateway + Login
# 2. Place order (demo)
# 3. Query order (demo)
# 4. Connect QuoteGateway + Subscribe
# 5. Listen for quote data (Enter to stop)
# 6. Disconnect all
# 0. Exit
```

**测试流程**:
1. 客户端选择 `1` — 连接 TradeGateway + 自动登录（admin/123456）
2. 客户端选择 `2` — 下单测试
3. 客户端选择 `4` — 连接 QuoteGateway + 订阅 "notice" 和 "market.BTC-USDT"
4. 客户端选择 `5` — 进入监听模式
5. 服务端选择 `1` — 输入通知内容 → 客户端收到通知（仅一条，已去重）
6. 服务端选择 `2` — 发布模拟行情 → 客户端收到行情数据
7. 客户端按 Enter 退出监听，选择 `6` 断开，`0` 退出
8. 服务端选择 `3` 或 `Ctrl+C` 优雅退出

## 集成使用

### 添加新业务网关（完整流程）

1. 创建 `your_gw/your_gw.proto` 定义协议
2. 创建 `your_gw/YourTypes.h` 定义类型+消息枚举
3. 创建 `your_gw/YourHandler.h` 定义服务端处理器接口
4. 创建 `your_gw/YourServer.h/.cpp` 继承 `framework::RouterServer` 或组合框架类
5. 创建 `your_gw/YourApi.h/.cpp` 继承 `framework::GatewayApi`
6. 在 `CMakeLists.txt` 中添加 proto 生成、库和链接
7. 在 `server.cpp` 中实例化并启动

### 添加新消息类型（以 TradeGateway 为例）

1. 在 `trade/trade.proto` 中定义新消息体
2. 在 `trade/TradeTypes.h` 的 `TradeMsgType` 枚举中添加新值
3. 在 `trade/TradeHandler.h` 中添加新虚函数
4. 在 `trade/trade_gateway.cpp` 的 `switch` 中添加 `case` 分支
5. 在 `trade/TradeApi.cpp` 中添加序列化/反序列化 + 发送/接收回调

## 框架基类说明

### RouterServer
- ROUTER socket 生命周期、dispatch 循环（zmq_poll 100ms 超时）
- 连接管理（最大连接数、空闲超时清理）
- Ping/Pong 心跳处理
- ThreadPool 集成
- 派生类实现 `OnMessage(identity, msgType, body)` + `SendReply()`

### PubServer
- PUB socket 生命周期
- `Publish(topic, msgType, body)` — 3 帧 PUB 消息
- Slow-joiner 缓解（3 次重发 + message_id 去重）

### GatewayApi
- DEALER + SUB 双 socket 管理
- `Connect(address, routerPort, pubPort, heartbeatSec)` — routerPort=0 为纯 SUB 模式，pubPort=0 为纯 DEALER 模式
- `Subscribe(topic)` / `Unsubscribe(topic)` — 本地 ZMQ SUB 订阅过滤，无服务端交互
- recv 循环（zmq_poll 双 socket）+ 心跳线程
- 派生类实现回调：`OnConnected/OnDisconnected/OnRouterMessage/OnPubMessage`

## 后续优化

| 方向 | 说明 |
|------|------|
| ~~框架抽象~~ | ✅ 完成：RouterServer/PubServer/GatewayApi 三大基类 |
| ~~TradeGateway~~ | ✅ 完成：登录/下单/查询/撤单 |
| ~~QuoteGateway~~ | ✅ 完成：主题订阅/行情数据/通知广播/去重 |
| **消息队列** | handler 与外部系统间引入消息队列 |
| **日志系统** | cerr/cout → spdlog |
| **配置化** | 端口/线程数/心跳参数迁移至 YAML |
| **单元测试** | gtest 覆盖 framework 和消息分派 |
| **安全** | bcrypt 密码 hash、ZMQ Curve 加密 |
| **Pipeline** | ZMQ PUSH/PULL 任务分发 |
