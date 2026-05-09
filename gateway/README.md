# Proto Gateway

基于 **C++17 / Protobuf / TCP** 的网络服务端框架。Gateway 模块接管 TCP 通信、消息收发及 protobuf 解析/归档，通过 `IGatewayHandler` 接口注入业务逻辑，实现通信层与业务层的解耦。

## 项目需求

- **Gateway 模块**：管理 TCP server 生命周期、客户端连接、消息收发、protobuf 解析与归档
- **业务注入**：提供 `IGatewayHandler` 接口，业务模块只需实现接口方法即可接入，无需关注网络细节
- **独立可拓展**：Gateway 与业务 handler 各自独立，新增消息类型/业务只需在接口中增加虚函数，并在 `ClientThreadFunc` 中增加分派分支
- **并发能力**：内建 `ThreadPool` 通用任务调度，可配置最大连接数、空闲超时回收
- **SDK 配套**：提供 `GatewayApi` 客户端 SDK，封装网络连接、消息收发、protobuf 转换，对业务层暴露纯 C++ 结构体回调

## 设计宗旨

```
┌──────────────────────────────────────────────────────────┐
│                     业务层（Business）                     │
│  ┌─────────────┐    ┌────────────────────────────────┐   │
│  │ AuthHandler │    │ MyGatewaySpi (client)           │   │
│  └──────┬──────┘    └──────────────┬─────────────────┘   │
├─────────┼──────────────────────────┼─────────────────────┤
│         │ IGatewayHandler          │ GatewaySpi          │
│         ▼                          ▼                    │
│  ┌───────────────────┐  ┌──────────────────────────┐    │
│  │    Gateway        │  │  GatewayApi (SDK)         │    │
│  │  ┌─────────────┐  │  │  (client)                 │    │
│  │  │ ThreadPool  │  │  └──────────────┬───────────┘    │
│  │  │ 连接管理     │  │                 │                │
│  │  └─────────────┘  │                 │                │
│  └────────┬──────────┘                 │                │
│           │                            │                │
│  ┌────────┴────────────────────────────┴─────────────┐  │
│  │         message_utils（TCP 收发包）                 │  │
│  │         gateway.proto（协议定义）                   │  │
│  └───────────────────────────────────────────────────┘  │
│                     通信层 / 协议层                       │
└──────────────────────────────────────────────────────────┘
```

核心原则：
- **通信与业务分离** — Gateway 只管理网络，业务只处理逻辑，互不侵入
- **接口驱动** — 服务端通过 `IGatewayHandler`、客户端通过 `GatewaySpi` 实现依赖反转
- **最小暴露** — protobuf 头文件仅限于实现文件内部，公共头文件不依赖 protobuf
- **可配置并发** — 线程池大小、最大连接数、空闲超时均可通过 Gateway 接口配置

## 项目结构

```
proto_gateway/
├── CMakeLists.txt              # 构建定义
├── README.md
├── proto/
│   └── gateway.proto           # protobuf 协议定义（LoginRequest/LoginResponse/AccountInfo）
│       └── generated/          # protoc 生成文件（gateway.pb.cc/h）
├── common/
│   ├── message_utils.h/.cpp    # TCP 收发包（长度前缀 + 消息类型）
│   ├── gateway_msg_types.h    # 消息类型枚举（client/server 共享）
│   └── ThreadPool.h/.cpp      # 通用固定大小任务线程池
├── server/
│   ├── Gateway.h               # IGatewayHandler 接口 + Gateway 抽象服务端类
│   ├── GatewayImpl.h/.cpp      # Gateway 实现（TCP server / 消息分派 / protobuf 解析 / 连接管理）
│   ├── AuthHandler.h/.cpp      # 登录认证业务（实现 IGatewayHandler）
│   └── server.cpp              # 服务端入口
├── api/
│   ├── GatewayApiDef.h         # 业务数据结构 + 错误码（与 protobuf 解耦）
│   ├── GatewayApi.h            # GatewaySpi 回调接口 + GatewayApi 客户端 SDK 接口
│   └── GatewayApiImpl.h/.cpp   # GatewayApi 实现（网络/线程/protobuf 转换）
├── client/
│   └── client.cpp              # 客户端交互式菜单 Demo
└── output/                     # 编译产物输出目录
    ├── server
    └── client
```

## 通信协议

### 消息格式（二进制）

```
┌────────────────┬──────────────────┬──────────────────────┐
│  4 bytes (BE)  │  2 bytes (BE)    │  N bytes             │
│  body_length   │  msg_type        │  protobuf payload    │
└────────────────┴──────────────────┴──────────────────────┘
```

`body_length` = protobuf 序列化后的字节长度（不含 type 字段自身）

### 消息类型

| 值 | 枚举 | 方向 | 说明 |
|----|------|------|------|
| `1` | `kLoginRequest` | Client → Server | 登录请求 |
| `2` | `kLoginResponse` | Server → Client | 登录响应 |
| _3-5_ | _预留_ | — | _QueryOrder/SendOrder 等后续拓展_ |

### Protobuf 定义（`proto/gateway.proto`）

```protobuf
message LoginRequest {
    uint64 request_id = 1;
    string username   = 2;
    string password   = 3;
}

message LoginResponse {
    uint64 request_id   = 1;
    int32  error_code   = 2;   // 0=成功
    string error_msg    = 3;
    AccountInfo account = 4;
}

message AccountInfo {
    uint64 user_id = 1;
    string username = 2;
    string email    = 3;
    string role     = 4;        // "admin" / "user"
}
```

## 实现要点

### 1. TCP 收发包（`message_utils`）

- **长度前缀定界**：4 字节大端 body 长度，先读长度再读 body，解决 TCP 粘包
- **消息类型分派**：2 字节大端 msg_type 位于长度与 body 之间，接收方可直接路由到对应 handler
- **循环读取**：`recv`/`send` 均封装为可应对短读写（short read/write）的完整读写函数

### 2. Gateway 服务端（`GatewayImpl`）

- **Accept + 线程池模型**：主 accept 线程阻塞等待连接，每个客户端任务提交到 `ThreadPool` 执行；若未设置线程池则退化为 `std::thread` per connection
- **消息分派**：`ClientThreadFunc` 循环调用 `recvMessage` 获取类型 + 负载，按 `switch (msgType)` 路由到不同 handler 方法
- **优雅退出**：`Stop()` 设置原子运行标志 → 关闭 server socket → 关闭所有 client socket → join accept 线程 → 线程池模式下清除追踪数据（池由调用者 `Shutdown`）
- **Socket 生命周期**：client 任务退出时在 `running_` guard 内关闭 socket 并标记共享列表为 -1，防止 `Stop()` 时重复 `close` 引发 fd 复用冲突；`Stop()` 路径下 socket 由 `CloseAllClientSockets` 统一关闭，client 任务跳过 `close`

### 3. 线程池（`ThreadPool`）

```cpp
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();  // 自动调用 Shutdown

    template <typename F>
    bool Submit(F&& task);  // 提交任务，池已关闭时返回 false

    void Shutdown();        // 停止接收，等待所有任务完成，join 工作线程
    size_t PendingTasks() const;
    size_t WorkerCount() const;
};
```

- 固定大小工作线程 + `std::queue<std::function<void()>>` 任务队列 + `std::condition_variable`
- `Submit` 为模板方法（头文件中），其余在 `.cpp` 中编译
- `Shutdown` 幂等：重复调用安全
- 独立于 Gateway，可被其他模块复用

### 4. 连接管理

| 功能 | 机制 | 配置 |
|------|------|------|
| **最大连接数** | `std::atomic<int>` 计数，accept 后 +1；超过阈值立即 close 并 reject | `SetMaxConnections(10000)`，0 表示不限制 |
| **空闲超时** | `setsockopt(SO_RCVTIMEO)` 在 OS 内核层执行，recv 超时 → `recvMessage` 失败 → 连接关闭 | `SetIdleTimeout(60)`，0 表示不启用 |
| **空闲回收** | 超时触发 → `ClientThreadFunc` 退出 → `activeConnections_--` → 资源释放 | 上述副作用，无需额外代码 |

设计决策：选用 `SO_RCVTIMEO` 而非 `poll`/`select` 循环，因为 (1) 零应用层轮询开销，(2) 无需修改 `message_utils`，(3) 小 protobuf 消息在超时窗口内必然到达完整。

### 5. 业务注入接口（`IGatewayHandler`）

```cpp
class IGatewayHandler {
public:
    virtual ~IGatewayHandler() = default;
    virtual bool OnLogin(const gateway_proto::LoginRequest& req,
                         gateway_proto::LoginResponse& rsp) = 0;
    // 后续拓展：
    // virtual bool OnQueryOrder(...) { return false; }
    // virtual bool OnSendOrder(...)  { return false; }
};
```

- handler 直接接收已解析的 protobuf 对象，无需处理字节流
- Gateway 负责在调用前后完成 protobuf ↔ bytes 转换及消息发送
- 新增业务消息类型：在 `IGatewayHandler` 中增加虚函数（可提供空默认实现），在 `ClientThreadFunc` 的 `switch` 中增加 `case` 分支即可

### 6. 客户端 SDK（`GatewayApi` / `GatewaySpi`）

- **接口分离**：`GatewayApi` 提供主动调用的 API（`Init`/`Login`/`Release`），`GatewaySpi` 提供异步回调（`OnFrontConnected`/`OnFrontDisconnected`/`OnLogin`）
- **protobuf 透明**：业务层仅使用纯 C++ 结构体（`LoginRequest`/`LoginResponse`/`AccountInfo`），protobuf 完全隐藏在 `GatewayApiImpl.cpp` 内部
- **TCP KeepAlive**：`Init` 时启用 TCP keepalive，心跳间隔可由业务层配置

## 构建 & 运行

### 依赖

- CMake >= 3.12
- Protocol Buffers（Homebrew：`brew install protobuf`）
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

终端 1 — 启动服务端：
```bash
./output/server
# 输出:
#   Gateway listening on 0.0.0.0:12345
#   Gateway server running with thread pool (4 workers). Press Ctrl+C to stop.
```

终端 2 — 启动客户端：
```bash
./output/client
# 交互式菜单:
#   ========== Gateway SDK Demo ==========
#   1. Login
#   2. Exit
#   ======================================
#   Please select: 1
#   Username: admin
#   Password: 123456
#
#   [MyGatewaySpi] OnFrontConnected
#   [Main] Login request sent, request_id=1
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

服务端 `Ctrl+C` 优雅退出。

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
    ThreadPool   pool(4);                // 4 个工作线程

    // 2. 创建 Gateway 并配置
    auto* gw = Gateway::Create();
    gw->RegisterHandler(&authHandler);   // 注入业务逻辑
    gw->SetThreadPool(&pool);            // 注入线程池
    gw->SetMaxConnections(10000);        // 最大并发连接数
    gw->SetIdleTimeout(60);              // 空闲 60s 自动回收

    // 3. 启动
    gw->Start("0.0.0.0", 12345);
    pause();  // 等待信号

    // 4. 关闭（注意顺序）
    gw->Stop();          // 停止接受新连接，关闭所有 socket
    pool.Shutdown();     // 等待所有 client 任务完成
    gw->Release();       // 安全释放
    return 0;
}
```

**关闭顺序说明**：`Stop()` → `pool.Shutdown()` → `Release()`。`Stop()` 关闭所有 socket 使阻塞中的任务快速退出；`Shutdown()` 等待所有任务线程返回，保证 `handler` 不再被引用；之后方可安全析构 handler 与 Gateway。

### 服务端：不使用线程池（简化模式）

```cpp
auto* gw = Gateway::Create();
gw->RegisterHandler(&handler);
// 不调用 SetThreadPool —— 退化为每个连接一个 std::thread
gw->Start("0.0.0.0", 12345);
pause();
gw->Stop();       // Stop 内部 join 所有 client 线程
gw->Release();
```

### 客户端：集成 SDK

```cpp
#include "api/GatewayApi.h"

class MySpi : public gateway::GatewaySpi {
    void OnLogin(const gateway::LoginResponse& rsp) override {
        if (rsp.error_code == gateway::ERR_OK) {
            // 登录成功，执行业务逻辑
        }
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

    // 异步等待回调 ...
    api->Release();
}
```

### 添加新消息类型（完整流程）

1. 在 `proto/gateway.proto` 中定义新消息体（如 `QueryOrderRequest`/`QueryOrderResponse`）
2. 在 `common/gateway_msg_types.h` 中添加枚举值 `kQueryOrderRequest`
3. 在 `server/Gateway.h` 的 `IGatewayHandler` 中添加虚函数 `OnQueryOrder`
4. 在 `server/GatewayImpl.cpp` 的 `ClientThreadFunc` 的 `switch` 中添加 `case` 分支
5. 在 `api/GatewayApiDef.h` 中添加对应的业务结构体（如需）
6. 在 `api/GatewayApi.h` 和 `GatewayApiImpl` 中添加对应的 API 方法

## 后续优化建议

| 方向 | 说明 |
|------|------|
| **IO 多路复用** | 将 accept + per-client 模型改为 `epoll`/`kqueue` 事件驱动，进一步提升 C10k+ 场景吞吐 |
| **消息队列** | 在 handler 与外部系统（如数据库、订单引擎）之间引入消息队列，解耦同步处理 |
| **日志系统** | 将 `std::cerr`/`std::cout` 替换为结构化日志（如 spdlog），支持级别过滤和文件输出 |
| **配置化** | 端口、地址、线程数、超时等硬编码参数迁移至配置文件（JSON/YAML） |
| **单元测试** | 对 `message_utils`、`ThreadPool`、`AuthHandler`、消息分派逻辑增加 gtest 覆盖 |
| **安全** | 登录密码 hash（bcrypt/argon2）、TLS 加密传输 |
| **协议兼容** | 考虑引入 protobuf `oneof` 包裹消息类型，替代独立的 2 字节 type 字段 |
| **应用层心跳** | Ping/Pong 消息替代纯 TCP 层面的 `SO_RCVTIMEO`，适应跨 NAT/代理等复杂网络环境 |
