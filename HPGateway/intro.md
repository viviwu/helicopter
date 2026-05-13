# HPGateway

## 项目简介

HPGateway 是一个基于 Linux + epoll 的高性能交易接入网关。

项目目标：

- 支持上千/上万客户端长连接
- 低延迟
- 高并发
- 稳定运行
- 面向交易系统场景
- 支持 Trade / Quote 双通道
- 提供类似 CTP 的 Api/Spi 风格接口

当前项目重点：

- Gateway 接入层
- Reactor 网络框架
- Connection 管理
- 异步消息收发
- 高性能网络模型

暂不涉及：

- 风控
- 撮合
- 数据库
- 策略系统

---

# 技术路线

## 开发环境

| 项目 | 说明 |
|---|---|
| OS | AlmaLinux 8 |
| Compiler | GCC 11 |
| Language | C++11 |
| Build | CMake |
| Network | epoll |
| Thread Model | one loop per thread |

---

# 网络架构

采用经典 Reactor 模型：

```text
                +-------------------+
                |    Acceptor       |
                +---------+---------+
                          |
                    new connection
                          |
          +---------------+---------------+
          |               |               |
    +-----v-----+   +-----v-----+   +-----v-----+
    | IOThread0 |   | IOThread1 |   | IOThreadN |
    +-----+-----+   +-----+-----+   +-----+-----+
          |               |               |
      TcpConnection   TcpConnection   TcpConnection
          |               |               |
          +---------------+---------------+
                          |
                    Business Queue
                          |
                    Worker Threads
```

---

# 线程模型

## IO线程

负责：

- accept
- recv/send
- epoll
- decode packet header
- heartbeat
- timeout

不负责：

- 业务逻辑
- 数据库
- 风控

---

## Business线程

负责：

- packet parse
- route
- logic process

---

# API设计

采用类似 CTP 的 Api/Spi 设计：

```text
Client
    ↓
TradeApi / QuoteApi
    ↓
GatewaySpi callback
```

支持：

- 登录
- 下单
- 撤单
- 查询
- 行情订阅
- Tick推送

---

# 目录结构

```text
include/
    gateway/
        GatewayApi.h
        GatewaySpi.h
        GatewayStruct.h

src/
    net/
    protocol/
    gateway/
    business/
```

---

# 核心技术点

## Reactor

- epoll ET
- non-blocking socket
- one loop per thread

## 高性能设计

- RingBuffer
- lockfree queue
- async send
- fixed packet header

## 稳定性

- 慢客户端隔离
- heartbeat
- reconnect
- async logger

---

# 后续规划

## Phase 1

- Reactor
- TcpServer
- Connection
- PacketCodec
- Api/Spi

## Phase 2

- ObjectPool
- TimerWheel
- Affinity
- SO_REUSEPORT

## Phase 3

- NUMA
- io_uring research
- DPDK/AF_XDP exploration

---

# License

MIT