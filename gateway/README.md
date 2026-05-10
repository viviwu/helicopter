# Proto Gateway (ZeroMQ)

基于 **C++17 / Protobuf / ZeroMQ** 的网络服务端框架。Gateway 模块接管 ZMQ 通信、消息收发及 protobuf 解析/归档，通过 `IGatewayHandler` 接口注入业务逻辑，实现通信层与业务层的解耦。

## 项目需求

- **Gateway 模块**：管理 ZMQ ROUTER socket 生命周期、客户端连接、消息收发、protobuf 解析与归档
- **业务注入**：提供 `IGatewayHandler` 接口，业务模块只需实现接口方法即可接入，无需关注网络细节
- **独立可拓展**：Gateway 与业务 handler 各自独立，新增消息类型/业务只需在接口中增加虚函数，并在分派分支中增加 case
- **并发能力**：内建 `ThreadPool` 通用任务调度，可配置最大连接数
- **连接管理**：支持最大连接数限制、空闲连接超时清理
- **双层心跳**：ZMQ 内置 `HEARTBEAT` + 应用层 `Ping/Pong`，跨 NAT/代理比纯 TCP keepalive 更可靠
- **服务端广播**：ZMQ `PUB/SUB` 模式实现服务端向所有已登录客户端推送通知
- **SDK 配套**：提供 `GatewayApi` 客户端 SDK，封装 ZMQ DEALER 连接、消息收发、protobuf 转换、自动心跳、广播接收与断线检测，对业务层暴露纯 C++ 结构体回调

## 设计宗旨

```
┌──────────────────────────────────────────────────────────────────┐
│                        业务层（Business）                          │
│  ┌─────────────┐    ┌─────────────────────────────────────────┐  │
│  │ AuthHandler │    │ MyGatewaySpi (client)                    │  │
│  └──────┬──────┘    └──────────────┬──────────────────────────┘  │
├─────────┼──────────────────────────┼────────────────────────────┤
│         │ IGatewayHandler          │ GatewaySpi                 │
│         ▼                          ▼                           │
│  ┌───────────────────┐  ┌─────────────────────────────────┐    │
│  │    Gateway        │  │  GatewayApi (SDK)                │    │
│  │  (ZMQ_ROUTER)     │  │  (ZMQ_DEALER + ZMQ_SUB)          │    │
│  │  (ZMQ_PUB)        │  │  ┌────────────┬────────────────┐ │    │
│  │  ┌─────────────┐  │  │  │ RecvThread │ HeartbeatThread│ │    │
│  │  │ ThreadPool  │  │  │  └────────────┴────────────────┘ │    │
│  │  │ 消息分派     │  │  └──────────────┬──────────────────┘    │
│  │  │ 广播推送     │  │                 │                       │
│  │  └─────────────┘  │                 │                       │
│  └────────┬──────────┘                 │                       │
│           │                            │                       │
│  ┌────────┴────────────────────────────┴────────────────────┐  │
│  │         message_utils（ZMQ 多部分消息收发）                 │  │
│  │         gateway.proto（协议定义）                          │  │
│  └──────────────────────────────────────────────────────────┘  │
│                     通信层 / 协议层                              │
└──────────────────────────────────────────────────────────────────┘
```

核心原则：
- **通信与业务分离** — Gateway 只管理网络，业务只处理逻辑，互不侵入
- **接口驱动** — 服务端通过 `IGatewayHandler`、客户端通过 `GatewaySpi` 实现依赖反转
- **最小暴露** — protobuf 头文件仅限于实现文件内部，公共头文件不依赖 protobuf
- **可配置并发** — 线程池大小、最大连接数均可通过 Gateway 接口配置
- **双通道设计** — DEALER/ROUTER 处理请求-应答，PUB/SUB 处理服务端主动推送

## 项目结构

```
gateway/
├── CMakeLists.txt              # 构建定义
├── README.md
├── proto/
│   └── gateway.proto           # protobuf 协议定义
│       └── generated/          # protoc 生成文件
├── common/
│   ├── message_utils.h/.cpp    # ZMQ 多部分消息收发（ROUTER/DEALER/PUB/SUB）
│   ├── gateway_msg_types.h    # 消息类型枚举
│   └── ThreadPool.h/.cpp      # 通用固定大小任务线程池
├── server/
│   ├── Gateway.h               # IGatewayHandler 接口 + Gateway 抽象服务端类
│   ├── GatewayImpl.h/.cpp      # Gateway 实现（ZMQ_ROUTER + ZMQ_PUB / 消息分派 / protobuf 解析）
│   ├── AuthHandler.h/.cpp      # 登录认证业务
│   └── server.cpp              # 服务端入口（含交互式广播菜单）
├── api/
│   ├── GatewayApiDef.h         # 业务数据结构 + 错误码
│   ├── GatewayApi.h            # GatewaySpi 回调接口 + GatewayApi SDK 接口
│   └── GatewayApiImpl.h/.cpp   # GatewayApi 实现（ZMQ_DEALER + ZMQ_SUB / protobuf 转换）
├── client/
│   └── client.cpp              # 客户端交互式菜单 Demo
└── output/                     # 编译产物输出目录
```

## 通信协议

### 消息格式（ZeroMQ 多部分消息）

使用 ZeroMQ `ROUTER/DEALER` 异步请求-应答模式，消息由 ZeroMQ 处理边界帧：

**服务端接收（ROUTER socket）：**
```
[identity frame]  [msg_type frame: 2 bytes]  [protobuf body]
```
- identity：ZeroMQ 自动为每个客户端分配的路由标识

**服务端发送（ROUTER socket）：**
```
[identity frame]  [msg_type frame: 2 bytes]  [protobuf body]
```

**客户端收发（DEALER socket）：**
```
[msg_type frame: 2 bytes]  [protobuf body]
```

**服务端广播（PUB socket）：**
```
[msg_type frame: 2 bytes]  [protobuf body]
```
- 第一条帧同时充当 ZMQ PUB/SUB 的订阅过滤主题（topic）
- 客户端 SUB socket 订阅空字符串 `""`，接收全部广播消息

### 消息类型

| 值 | 枚举 | 方向 | 说明 |
|----|------|------|------|
| `1` | `kLoginRequest` | Client → Server | 登录请求 |
| `2` | `kLoginResponse` | Server → Client | 登录响应 |
| `3` | `kPing` | Client → Server | 应用层心跳请求 |
| `4` | `kPong` | Server → Client | 应用层心跳响应 |
| `5` | `kBroadcastNotification` | Server → Client（PUB） | 服务端广播通知 |

### 与旧版 TCP 协议的关键区别

| 特性 | 旧版（raw TCP） | 新版（ZeroMQ） |
|------|----------------|---------------|
| 消息帧 | 手工 4 字节长度前缀 + 循环 readFull | ZeroMQ 自动处理 |
| IO 模型 | accept + per-connection 线程 | 单 dispatch 线程 + zmq_poll |
| 连接管理 | 手工管理 clientSockets_ 列表 | ZeroMQ 自动管理 identity 路由 |
| 重连 | 不支持 | DEALER 自动重连 |
| 优雅关闭 | SO_RCVTIMEO + close 所有 fd | zmq_poll 超时 + running_ 标志 |
| 心跳机制 | TCP keepalive（NAT 穿透差） | ZMQ_HEARTBEAT + 应用层 Ping/Pong |
| 服务端推送 | 不支持 | PUB/SUB 广播模式 |

## 实现要点

### 1. ZMQ 消息收发（`message_utils`）

- **进程级 ZMQ Context**：通过 `GetZmqContext()` 获取共享 context，全进程所有 socket 共用
- **ROUTER 端收发**：`RecvRouterMessage` / `SendRouterMessage` 处理带 identity 的三帧消息
- **DEALER 端收发**：`RecvMessage` / `SendMessage` 处理两帧消息
- **PUB/SUB 支持**：`CreatePubSocket` / `CreateSubSocket` 创建广播/订阅 socket；`SetSubSubscribe` 设置订阅过滤器
- **Socket 生命周期管理**：创建时自动设置 linger=0；`CloseSocket` 安全释放

### 2. Gateway 服务端（`GatewayImpl`）

- **单 dispatch 线程模型**：`DispatchThreadFunc` 使用 `zmq_poll` (100ms 超时) 轮询 ROUTER socket，收到消息后提交到 ThreadPool 处理
- **消息分派**：`ProcessMessage` 按 `switch (msgType)` 路由到不同 handler 方法
- **连接数控制**：通过跟踪 client identity 集合限制最大连接数，超限时返回错误响应
- **空闲连接清理**：`idleTimeoutSec_` 超时未活跃的 identity 自动清理，释放连接计数
- **心跳响应**：dispatch 线程直接处理 `kPing` 并回复 `kPong`，不经过线程池，降低延迟
- **广播推送**：`BroadcastNotification` 将 protobuf 消息通过 PUB socket 发送，内建 slow-joiner 缓解策略（3 次重发，间隔 100ms）
- **优雅退出**：`Stop()` 设置 `running_=false` → join dispatch 线程（最多 100ms）→ 关闭 socket → 调用者 `pool.Shutdown()` → `Release()`

### 3. Pub-Sub 广播机制

#### 架构

```
┌─────────────────┐         ┌──────────────────────────────┐
│   Server Menu   │         │   Client RecvThread          │
│                 │         │                              │
│ BroadcastNotif  │  PUB    │  zmq_poll([DEALER, SUB])     │
│      ↓          │ ──────→ │       ↓                      │
│  pubSock_       │ :12346  │   subSock_ (:12346)          │
│                 │         │       ↓                      │
│                 │         │  RecvMessage → kBroadcast    │
│                 │         │       ↓                      │
│                 │         │  OnBroadcast callback        │
└─────────────────┘         └──────────────────────────────┘
```

#### 关键设计决策

- **PUB 端口 = ROUTER 端口 + 1**：服务端在 `routerPort + 1` 上绑定 PUB socket，客户端自动计算 SUB 连接地址
- **订阅过滤器**：客户端 SUB socket 订阅 `""`（空字符串），接收所有广播消息，不过滤
- **Slow-joiner 缓解**：ZMQ PUB/SUB 存在 slow-joiner 问题 — SUB socket 从 `zmq_connect` 到订阅过滤器同步至 PUB 需要短暂时间，期间发送的消息会被丢弃。服务端每条广播连续发送 3 次（间隔 100ms），确保新上线的客户端至少能收到一次
- **独立于请求-应答通道**：PUB/SUB 与 ROUTER/DEALER 是独立的 socket 和端口，广播不经过线程池，也不依赖客户端 identity
- **单向推送**：PUB socket 仅用于发送，不接收数据；SUB socket 仅用于接收，不发送数据

#### Slow-joiner 问题说明

ZMQ PUB/SUB 的 slow-joiner 是固有设计特性：SUB 端的 `zmq_setsockopt(ZMQ_SUBSCRIBE)` 是异步操作，订阅信息需要通过网络传输到 PUB 端后才能生效。在同步完成前，PUB 发送的消息对新的 SUB 不可见。

本项目的缓解策略（重发 3 次 × 100ms 间隔）适用于演示场景。生产环境建议：
- 使用 ZMQ socket monitor 检测 SUB 连接事件后再发送
- 在 SUB 端 `zmq_connect` 后增加 200ms 的 `zmq_sleep` 等待同步
- 或改用 `ZMQ_XPUB` / `ZMQ_XSUB` 手动管理订阅

### 4. 线程池（`ThreadPool`）

```cpp
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();  // 自动调用 Shutdown

    template <typename F>
    bool Submit(F&& task);
    void Shutdown();
    size_t PendingTasks() const;
    size_t WorkerCount() const;
};
```

- 固定大小工作线程 + `std::queue<std::function<void()>>` + `std::condition_variable`
- `Submit` 为模板方法（头文件中），其余在 `.cpp` 中编译
- `Shutdown` 幂等：重复调用安全

### 5. 业务注入接口（`IGatewayHandler`）

```cpp
class IGatewayHandler {
public:
    virtual ~IGatewayHandler() = default;
    virtual bool OnLogin(const gateway_proto::LoginRequest& req,
                         gateway_proto::LoginResponse& rsp) = 0;
};
```

- handler 直接接收已解析的 protobuf 对象，无需处理字节流
- Gateway 负责在调用前后完成 protobuf ↔ bytes 转换及消息发送

### 6. 客户端 SDK（`GatewayApi` / `GatewaySpi`）

- **接口分离**：`GatewayApi` 提供主动调用的 API（`Init`/`Login`/`Release`），`GatewaySpi` 提供异步回调
- **protobuf 透明**：业务层仅使用纯 C++ 结构体，protobuf 完全隐藏在 `GatewayApiImpl.cpp` 内部
- **ZMQ_DEALER + ZMQ_SUB**：客户端同时持有 DEALER socket（请求-应答）和 SUB socket（广播接收）；接收线程使用 `zmq_poll` 同时监听两个 socket
- **双层心跳**：
  - **ZMQ 层**：socket 启用 `ZMQ_HEARTBEAT_IVL` / `ZMQ_HEARTBEAT_TIMEOUT`，在 ZMTP 协议层维持连接，穿越 NAT/代理能力优于 TCP keepalive
  - **应用层**：独立心跳线程按 `heartbeatIntervalSec` 周期性发送 `kPing`；接收线程收到 `kPong` 后重置计时器；连续 3 个周期未收到 `kPong` 触发 `OnFrontDisconnected(1)`（心跳超时）
- **TCP KeepAlive**：保留作为旧版 ZMQ 兜底方案
- **广播回调**：`OnBroadcast` 异步回调，在接收线程中触发

## 构建 & 运行

### 依赖

- CMake >= 3.12
- Protocol Buffers（Homebrew：`brew install protobuf`）
- ZeroMQ 4.x（Homebrew：`brew install zeromq`）
- C++17 编译器（Apple Clang / GCC）

### 构建

```bash
cmake -B cmake-build-debug -S .
cmake --build cmake-build-debug
```

产物输出至 `output/` 目录：
```
output/server    # 服务端可执行文件
output/client    # 客户端可执行文件
```

### 运行测试

#### 基础登录测试

终端 1 — 启动服务端：
```bash
./output/server
# 输出:
#   Gateway (ZMQ_ROUTER) listening on tcp://0.0.0.0:12345
#   Gateway (ZMQ_PUB)    listening on tcp://0.0.0.0:12346
#   Gateway server running with thread pool (4 workers).
#   ========== Gateway Server Menu ==========
#   1. Broadcast notification to all clients
#   2. Shutdown server
#   =========================================
#   Please select:
```

终端 2 — 启动客户端：
```bash
./output/client
# 交互式菜单:
#   ========== Gateway SDK Demo ==========
#   Status: Disconnected
#   --------------------------------------
#   1. Connect to server
#   2. Login
#   3. Listen for broadcasts (press Enter to stop)
#   4. Disconnect
#   0. Exit
#   ======================================
#   Please select: 1
#
#   [MyGatewaySpi] OnFrontConnected
#   Connected to server (DEALER:12345 + SUB:12346).
#
#   Please select: 2
#   Username: admin
#   Password: 123456
#
#   [MyGatewaySpi] OnLogin callback received:
#     request_id : 1
#     error_code : 0 (OK)
#     error_msg  : Login successful
#     account.user_id  : 1001
#     account.username : admin
#     account.email    : admin@example.com
#     account.role     : admin
```

#### 广播推送测试

客户端登录后选择 `3` 进入监听模式，然后在服务端发送广播：

服务端：
```
Please select: 1
Enter broadcast message: Hello all clients!
Broadcast sent to all clients: Hello all clients!
```

客户端将收到：
```
[MyGatewaySpi] >>> BROADCAST received <<<
  content   : Hello all clients!
  timestamp : 1715337600000
Press Enter to continue...
```

服务端 `Ctrl+C` 或选择 `2` 优雅退出。

## 集成使用

### 服务端：完整配置示例

```cpp
#include "server/Gateway.h"
#include "server/AuthHandler.h"
#include "common/ThreadPool.h"

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 1. 创建业务 handler 和线程池
    AuthHandler  authHandler;
    ThreadPool   pool(4);

    // 2. 创建 Gateway 并配置
    auto* gw = Gateway::Create();
    gw->RegisterHandler(&authHandler);
    gw->SetThreadPool(&pool);
    gw->SetMaxConnections(10000);
    gw->SetIdleTimeout(60);   // 60 秒无消息则清理连接

    // 3. 启动（同时启动 ROUTER 和 PUB socket）
    gw->Start("0.0.0.0", 12345);

    // 4. 发送广播
    gw->BroadcastNotification("Server maintenance in 5 minutes");

    // 5. 等待关闭信号
    while (!g_shutdownRequested) pause();

    // 6. 关闭（注意顺序）
    gw->Stop();          // 通知 dispatch 线程退出（最多 100ms）
    pool.Shutdown();     // 等待所有 client 任务完成
    gw->Release();       // 安全释放
    return 0;
}
```

**关闭顺序说明**：`Stop()` → `pool.Shutdown()` → `Release()`。`Stop()` 设置 running 标志并 join dispatch 线程；`Shutdown()` 等待所有任务线程返回；之后方可安全析构 handler 与 Gateway。

### 服务端：不使用线程池（简化模式）

```cpp
auto* gw = Gateway::Create();
gw->RegisterHandler(&handler);
// 不调用 SetThreadPool —— 消息在 dispatch 线程中同步处理
gw->Start("0.0.0.0", 12345);
gw->BroadcastNotification("Hello");
pause();
gw->Stop();
gw->Release();
```

### 客户端：集成 SDK

```cpp
#include "api/GatewayApi.h"

class MySpi : public gateway::GatewaySpi {
    void OnFrontConnected() override {
        // 连接建立（Init 成功后同步回调）
    }

    void OnFrontDisconnected(int nReason) override {
        // nReason: 0=正常断开/网络错误, 1=应用层心跳超时
        // 可在此触发重连逻辑
    }

    void OnLogin(const gateway::LoginResponse& rsp) override {
        if (rsp.error_code == gateway::ERR_OK) {
            // 登录成功，执行业务逻辑
        }
    }

    void OnBroadcast(const gateway::BroadcastNotification& notif) override {
        // 收到服务端广播通知（PUB/SUB 通道）
        std::cout << "Broadcast: " << notif.content << std::endl;
    }
};

int main() {
    auto* api = gateway::GatewayApi::CreateGatewayApi();
    MySpi spi;
    api->RegisterSpi(&spi);
    api->Init("127.0.0.1", 12345, 5);

    gateway::LoginRequest req;
    req.username = "admin";
    req.password = "123456";
    api->Login(req);

    // 异步等待回调（广播通过 OnBroadcast 推送）...
    api->Release();
}
```

### 添加新消息类型（完整流程）

1. 在 `proto/gateway.proto` 中定义新消息体
2. 在 `common/gateway_msg_types.h` 中添加枚举值
3. 在 `server/Gateway.h` 的 `IGatewayHandler` 中添加虚函数（如需服务端处理）
4. 在 `server/GatewayImpl.cpp` 的 `ProcessMessage` 的 `switch` 中添加 `case` 分支
5. 在 `api/GatewayApiDef.h` 中添加对应的业务结构体（如需）
6. 在 `api/GatewayApi.h` 和 `GatewayApiImpl` 中添加对应的 API 方法

## 后续优化建议

| 方向 | 说明 |
|------|------|
| ~~应用层心跳~~ | ✅ 已完成：ZMQ_HEARTBEAT + 自定义 Ping/Pong 双层心跳，跨 NAT/代理更可靠 |
| ~~Pub-Sub 广播~~ | ✅ 已完成：ZMQ PUB/SUB 模式实现服务端消息广播，含 slow-joiner 缓解 |
| **消息队列** | 在 handler 与外部系统间引入消息队列，解耦同步处理 |
| **日志系统** | 将 `std::cerr`/`std::cout` 替换为结构化日志（如 spdlog） |
| **配置化** | 端口、地址、线程数、心跳间隔（ZMQ_HEARTBEAT_IVL / 应用层 Ping 周期）等硬编码参数迁移至配置文件（JSON/YAML） |
| **单元测试** | 对 `message_utils`、`ThreadPool`、消息分派逻辑增加 gtest 覆盖 |
| **安全** | 登录密码 hash（bcrypt/argon2）、ZMQ Curve 加密传输 |
| **Pipeline 模式** | 利用 ZeroMQ PUSH/PULL 实现任务分发与负载均衡 |
| **广播可靠性** | 引入 ZMQ_XPUB socket monitor 检测订阅者，替代重发策略；或接入 Redis Pub/Sub 做跨进程桥接 |
