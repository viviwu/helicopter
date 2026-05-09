# Proto Gateway (ZeroMQ)

基于 **C++17 / Protobuf / ZeroMQ** 的网络服务端框架。Gateway 模块接管 ZMQ 通信、消息收发及 protobuf 解析/归档，通过 `IGatewayHandler` 接口注入业务逻辑，实现通信层与业务层的解耦。

## 项目需求

- **Gateway 模块**：管理 ZMQ ROUTER socket 生命周期、客户端连接、消息收发、protobuf 解析与归档
- **业务注入**：提供 `IGatewayHandler` 接口，业务模块只需实现接口方法即可接入，无需关注网络细节
- **独立可拓展**：Gateway 与业务 handler 各自独立，新增消息类型/业务只需在接口中增加虚函数，并在分派分支中增加 case
- **并发能力**：内建 `ThreadPool` 通用任务调度，可配置最大连接数
- **SDK 配套**：提供 `GatewayApi` 客户端 SDK，封装 ZMQ DEALER 连接、消息收发、protobuf 转换，对业务层暴露纯 C++ 结构体回调

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
│  │  (ZMQ_ROUTER)     │  │  (ZMQ_DEALER)             │    │
│  │  ┌─────────────┐  │  └──────────────┬───────────┘    │
│  │  │ ThreadPool  │  │                 │                │
│  │  │ 消息分派     │  │                 │                │
│  │  └─────────────┘  │                 │                │
│  └────────┬──────────┘                 │                │
│           │                            │                │
│  ┌────────┴────────────────────────────┴─────────────┐  │
│  │         message_utils（ZMQ 多部分消息收发）          │  │
│  │         gateway.proto（协议定义）                   │  │
│  └───────────────────────────────────────────────────┘  │
│                     通信层 / 协议层                       │
└──────────────────────────────────────────────────────────┘
```

核心原则：
- **通信与业务分离** — Gateway 只管理网络，业务只处理逻辑，互不侵入
- **接口驱动** — 服务端通过 `IGatewayHandler`、客户端通过 `GatewaySpi` 实现依赖反转
- **最小暴露** — protobuf 头文件仅限于实现文件内部，公共头文件不依赖 protobuf
- **可配置并发** — 线程池大小、最大连接数均可通过 Gateway 接口配置

## 项目结构

```
gateway/
├── CMakeLists.txt              # 构建定义
├── README.md
├── proto/
│   └── gateway.proto           # protobuf 协议定义
│       └── generated/          # protoc 生成文件
├── common/
│   ├── message_utils.h/.cpp    # ZMQ 多部分消息收发（ROUTER/DEALER）
│   ├── gateway_msg_types.h    # 消息类型枚举
│   └── ThreadPool.h/.cpp      # 通用固定大小任务线程池
├── server/
│   ├── Gateway.h               # IGatewayHandler 接口 + Gateway 抽象服务端类
│   ├── GatewayImpl.h/.cpp      # Gateway 实现（ZMQ_ROUTER / 消息分派 / protobuf 解析）
│   ├── AuthHandler.h/.cpp      # 登录认证业务
│   └── server.cpp              # 服务端入口
├── api/
│   ├── GatewayApiDef.h         # 业务数据结构 + 错误码
│   ├── GatewayApi.h            # GatewaySpi 回调接口 + GatewayApi SDK 接口
│   └── GatewayApiImpl.h/.cpp   # GatewayApi 实现（ZMQ_DEALER / protobuf 转换）
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

### 消息类型

| 值 | 枚举 | 方向 | 说明 |
|----|------|------|------|
| `1` | `kLoginRequest` | Client → Server | 登录请求 |
| `2` | `kLoginResponse` | Server → Client | 登录响应 |
| _3-5_ | _预留_ | — | _后续拓展_ |

### 与旧版 TCP 协议的关键区别

| 特性 | 旧版（raw TCP） | 新版（ZeroMQ） |
|------|----------------|---------------|
| 消息帧 | 手工 4 字节长度前缀 + 循环 readFull | ZeroMQ 自动处理 |
| IO 模型 | accept + per-connection 线程 | 单 dispatch 线程 + zmq_poll |
| 连接管理 | 手工管理 clientSockets_ 列表 | ZeroMQ 自动管理 identity 路由 |
| 重连 | 不支持 | DEALER 自动重连 |
| 优雅关闭 | SO_RCVTIMEO + close 所有 fd | zmq_poll 超时 + running_ 标志 |

## 实现要点

### 1. ZMQ 消息收发（`message_utils`）

- **进程级 ZMQ Context**：通过 `GetZmqContext()` 获取共享 context，全进程所有 socket 共用
- **ROUTER 端收发**：`RecvRouterMessage` / `SendRouterMessage` 处理带 identity 的三帧消息
- **DEALER 端收发**：`RecvMessage` / `SendMessage` 处理两帧消息
- **Socket 生命周期管理**：`CreateRouterSocket` / `CreateDealerSocket` 自动设置 linger=0；`CloseSocket` 安全释放

### 2. Gateway 服务端（`GatewayImpl`）

- **单 dispatch 线程模型**：`DispatchThreadFunc` 使用 `zmq_poll` (100ms 超时) 轮询 ROUTER socket，收到消息后提交到 ThreadPool 处理
- **消息分派**：`ProcessMessage` 按 `switch (msgType)` 路由到不同 handler 方法
- **连接数控制**：通过跟踪 client identity 集合限制最大连接数，超限时返回错误响应
- **优雅退出**：`Stop()` 设置 `running_=false` → join dispatch 线程（最多 100ms）→ 关闭 socket → 调用者 `pool.Shutdown()` → `Release()`

### 3. 线程池（`ThreadPool`）

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

### 4. 业务注入接口（`IGatewayHandler`）

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

### 5. 客户端 SDK（`GatewayApi` / `GatewaySpi`）

- **接口分离**：`GatewayApi` 提供主动调用的 API（`Init`/`Login`/`Release`），`GatewaySpi` 提供异步回调
- **protobuf 透明**：业务层仅使用纯 C++ 结构体，protobuf 完全隐藏在 `GatewayApiImpl.cpp` 内部
- **ZMQ_DEALER**：客户端使用 DEALER socket，自动处理重连；接收线程使用 `zmq_poll` (100ms) 支持优雅退出
- **TCP KeepAlive**：通过 `ZMQ_TCP_KEEPALIVE` 选项启用

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

终端 1 — 启动服务端：
```bash
./output/server
# 输出:
#   Gateway (ZMQ_ROUTER) listening on tcp://0.0.0.0:12345
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
    ThreadPool   pool(4);

    // 2. 创建 Gateway 并配置
    auto* gw = Gateway::Create();
    gw->RegisterHandler(&authHandler);
    gw->SetThreadPool(&pool);
    gw->SetMaxConnections(10000);

    // 3. 启动
    gw->Start("0.0.0.0", 12345);

    // 4. 等待关闭信号
    while (!g_shutdownRequested) pause();

    // 5. 关闭（注意顺序）
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
pause();
gw->Stop();
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

1. 在 `proto/gateway.proto` 中定义新消息体
2. 在 `common/gateway_msg_types.h` 中添加枚举值
3. 在 `server/Gateway.h` 的 `IGatewayHandler` 中添加虚函数
4. 在 `server/GatewayImpl.cpp` 的 `ProcessMessage` 的 `switch` 中添加 `case` 分支
5. 在 `api/GatewayApiDef.h` 中添加对应的业务结构体（如需）
6. 在 `api/GatewayApi.h` 和 `GatewayApiImpl` 中添加对应的 API 方法

## 后续优化建议

| 方向 | 说明 |
|------|------|
| **应用层心跳** | 使用 ZMQ_HEARTBEAT 或自定义 Ping/Pong 消息替代 TCP keepalive，跨 NAT/代理更可靠 |
| **消息队列** | 在 handler 与外部系统间引入消息队列，解耦同步处理 |
| **日志系统** | 将 `std::cerr`/`std::cout` 替换为结构化日志（如 spdlog） |
| **配置化** | 端口、地址、线程数等硬编码参数迁移至配置文件（JSON/YAML） |
| **单元测试** | 对 `message_utils`、`ThreadPool`、消息分派逻辑增加 gtest 覆盖 |
| **安全** | 登录密码 hash（bcrypt/argon2）、ZMQ Curve 加密传输 |
| **Pub-Sub 广播** | 利用 ZeroMQ PUB/SUB 模式实现服务端消息广播 |
| **Pipeline 模式** | 利用 ZeroMQ PUSH/PULL 实现任务分发与负载均衡 |
