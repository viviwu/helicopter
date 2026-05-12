# 交易系统工程化：HPGateway设计与实现路线

要求：
* Linux（AlmaLinux 8）
* 稳定优先
* 成熟架构
* C++11 ~ C++17
* Gateway层
* 高并发连接
* CTP风格Api/Spi
* 不追求炫技

这其实已经把路线限定得非常清晰：

“传统 Reactor + epoll + one-loop-per-thread + 业务解耦”的工业级方案。

这也是目前大量：

* 券商柜台
* 行情前置
* 柜台接入层
* 量化交易接入
* 游戏网关

仍然在使用的主流架构。

而不是：

io_uring / coroutine / DPDK / Seastar

这些“更先进但工程风险更高”的路线。

我这个选择非常合理。

⸻

一、我建议的最终技术路线

我会这样定：

模块	技术
OS	AlmaLinux 8
编译器	gcc 11
C++标准	C++11（主体）+ 少量C++14/17可选
构建	CMake
网络模型	epoll ET
架构	Reactor
线程模型	one loop per thread
API风格	CTP Api/Spi
Buffer	RingBuffer
Queue	MPSC lockfree
日志	spdlog（异步）
定时器	timerfd
wakeup	eventfd
内存管理	对象池
协议	固定二进制包头
生命周期	非shared_ptr重度依赖

⸻

二、Gateway的定位（非常关键）

我们要明确：

⸻

Gateway不是业务系统

Gateway只负责：

收包
解包
连接管理
心跳
流控
转发
发送

不要：

数据库
风控
复杂业务
策略
撮合

否则：

尾延迟会炸

⸻

三、我建议的整体项目结构

这里开始正式工程设计。

⸻

目录结构（推荐）

gateway/
├── CMakeLists.txt
├── build.sh
├── conf/
│   └── gateway.yaml
│
├── include/
│   └── gateway/
│       ├── GatewayApi.h
│       ├── GatewaySpi.h
│       ├── GatewayStruct.h
│       └── GatewayError.h
│
├── src/
│
│   ├── net/
│   │   ├── Acceptor.h
│   │   ├── Acceptor.cpp
│   │   ├── EventLoop.h
│   │   ├── EventLoop.cpp
│   │   ├── Channel.h
│   │   ├── Channel.cpp
│   │   ├── Poller.h
│   │   ├── Poller.cpp
│   │   ├── TcpConnection.h
│   │   ├── TcpConnection.cpp
│   │   ├── TcpServer.h
│   │   ├── TcpServer.cpp
│   │   ├── Buffer.h
│   │   ├── Buffer.cpp
│   │   ├── SocketUtil.h
│   │   └── SocketUtil.cpp
│
│   ├── base/
│   │   ├── NonCopyable.h
│   │   ├── Timestamp.h
│   │   ├── Logger.h
│   │   ├── Logger.cpp
│   │   ├── Thread.h
│   │   ├── Thread.cpp
│   │   ├── ThreadPool.h
│   │   ├── ThreadPool.cpp
│   │   ├── LockFreeQueue.h
│   │   └── RingBuffer.h
│
│   ├── protocol/
│   │   ├── Packet.h
│   │   ├── Codec.h
│   │   ├── Codec.cpp
│   │   ├── PacketDispatcher.h
│   │   └── PacketDispatcher.cpp
│
│   ├── gateway/
│   │   ├── GatewayServer.h
│   │   ├── GatewayServer.cpp
│   │   ├── SessionManager.h
│   │   ├── SessionManager.cpp
│   │   ├── ConnectionManager.h
│   │   ├── ConnectionManager.cpp
│   │   ├── GatewayApiImpl.h
│   │   ├── GatewayApiImpl.cpp
│   │   ├── GatewayContext.h
│   │   └── GatewayContext.cpp
│
│   ├── business/
│   │   ├── WorkerPool.h
│   │   ├── WorkerPool.cpp
│   │   ├── MessageRouter.h
│   │   └── MessageRouter.cpp
│
│   └── main.cpp
│
├── thirdparty/
│   ├── spdlog/
│   └── concurrentqueue/
│
└── tests/

⸻

四、为什么这样拆（这是关键）

很多人会：

TcpServer.cpp 5000行

然后：

根本无法演化

交易系统：

必须：

网络层
协议层
业务层
API层
彻底解耦

否则后期：

* 延迟问题
* 粘包问题
* 风控插入
* 热更新

全部痛苦。

⸻

五、CTP风格 API/SPI 设计（重点）

我这个方向非常对。

因为：

交易系统最怕 callback chaos（回调混乱）

CTP模式：

Api主动调用
Spi被动通知

实际上非常适合交易接入。

⸻

GatewaySpi

例如：

class GatewaySpi {
public:
virtual void OnConnected() {}
virtual void OnDisconnected(int reason) {}
virtual void OnClientLogin(
const LoginReq* req,
int connId) {}
virtual void OnMessage(
const Packet* packet,
int connId) {}
virtual void OnError(
int errorCode,
const char* msg) {}
};

⸻

GatewayApi

class GatewayApi {
public:
virtual void RegisterSpi(GatewaySpi* spi) = 0;
virtual bool Start() = 0;
virtual void Stop() = 0;
virtual bool SendToClient(
int connId,
const void* data,
uint32_t len) = 0;
virtual bool Broadcast(
const void* data,
uint32_t len) = 0;
};

⸻

六、为什么CTP模式适合Gateway

因为：

Gateway本质：

事件驱动

例如：

OnConnect
OnDisconnect
OnPacket
OnHeartbeatTimeout

天然：

Spi callback

模型。

⸻

七、网络线程与业务线程关系（重点）

这是交易系统核心。

⸻

网络线程

只做：

recv
decode header
push queue
send

⸻

业务线程

做：

packet parse
route
logic
risk

⸻

正确流程

Client
↓
IO Thread
↓
Decode Header
↓
Push Business Queue
↓
Worker Thread
↓
Business Logic
↓
Push SendQueue
↓
IO Thread
↓
send()

⸻

八、TcpConnection（重点设计）

这是核心对象。

⸻

Connection对象绝不能太胖

我会这样：

class TcpConnection {
public:
int fd;
uint64_t connId;
EventLoop* loop;
RingBuffer input;
RingBuffer output;
std::atomic<int> state;
sockaddr_in peer;
Timestamp lastRecvTime;
Timestamp lastSendTime;
};

⸻

九、Connection生命周期

不要：

shared_ptr everywhere

因为：

atomic refcount
=
cache bouncing

⸻

我建议

⸻

方案1（推荐）

IO线程独占Connection生命周期。

业务线程：

不持有Connection。

只持有：

connId

发送时：

connId -> lookup

这是大量交易系统的做法。

⸻

十、Packet协议设计（关键）

⸻

固定头

#pragma pack(push, 1)
struct PacketHeader {
uint32_t length;
uint16_t cmd;
uint16_t flag;
uint32_t seq;
};
#pragma pack(pop)

⸻

为什么不用protobuf直接裸传

因为：

Gateway层：

最重要：

快速识别包边界

而不是：

复杂schema

⸻

十一、Buffer设计（关键）

不要：

std::string += recv

交易系统里不行。

⸻

推荐RingBuffer

因为：

* 无内存搬移
* cache友好
* 高吞吐
* 长连接友好

⸻

十二、为什么不用协程？

我可能会想到：

asio coroutine
libco
boost coroutine

但：

我建议：

⸻

第一版别碰协程

原因：

⸻

1. 调试复杂

交易系统：

稳定 > 炫技

⸻

2. Linux版本兼容

我已经明确：

兼容性优先

⸻

3. epoll线程模型已经足够强

对于：

长连接
小包
高并发

epoll已经极强。

⸻

十三、线程数怎么定

⸻

IO线程

建议：

CPU核心数 / 2

例如：

8核：

4 IO threads

⸻

Business线程

根据：

业务复杂度

调整。

⸻

十四、CMake（推荐）

⸻

顶层

cmake_minimum_required(VERSION 3.10)
project(gateway)
set(CMAKE_CXX_STANDARD 11)
add_compile_options(
-O2
-g
-Wall
-Wextra
-Wno-unused-parameter
)
add_subdirectory(src)

⸻

Linux编译参数

推荐：

-pthread

以及：

-fno-omit-frame-pointer

方便perf分析。

⸻

十五、我建议第一阶段目标

不要一上来：

极限低延迟

⸻

第一阶段目标

只做：

⸻

功能

* accept连接
* epoll
* ET模式
* 非阻塞
* Connection管理
* RingBuffer
* Packet decode
* worker queue
* async send
* Api/Spi

⸻

压测目标

例如：

5000连接
每秒10万消息
稳定运行

⸻

十六、后续升级路线（以后）

后续再逐渐加入：

⸻

第二阶段

* 内存池
* slab
* timer wheel
* lockfree send queue
* affinity
* SO_REUSEPORT

⸻

第三阶段

* NUMA
* busy poll
* io_uring实验
* protobuf zero copy
* kernel bypass

⸻

十七、接下来最合理的实践路线

我建议下一步直接开始：

⸻

第一批核心模块实现

按顺序：

⸻

Step1

基础：

NonCopyable
Thread
Logger
Timestamp

⸻

Step2

Reactor：

Channel
Poller
EventLoop

⸻

Step3

网络：

Acceptor
TcpConnection
TcpServer

⸻

Step4

协议：

Packet
Codec
Dispatcher

⸻

Step5

Gateway：

GatewayApi
GatewaySpi
SessionManager

⸻

Step6

压测：

10k client benchmark

⸻

这条路线非常稳，也非常“真正工程化”。