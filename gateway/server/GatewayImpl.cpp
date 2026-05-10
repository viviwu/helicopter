/**
  ******************************************************************************
  * @file           : GatewayImpl.cpp
  * @author         : vivi wu
  * @brief          : Gateway 服务端实现（ZMQ_ROUTER）
  * @version        : 0.2.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#include "server/GatewayImpl.h"
#include "common/message_utils.h"
#include "gateway.pb.h"

#include <iostream>
#include <set>
#include <string>

#include <zmq.h>

namespace gateway {

// ============================================================================
// 工厂方法
// ============================================================================
Gateway* Gateway::Create() {
    return new GatewayImpl();
}

// ============================================================================
// 构造/析构
// ============================================================================
GatewayImpl::GatewayImpl() = default;

GatewayImpl::~GatewayImpl() {
    Stop();
}

// ============================================================================
// 生命周期
// ============================================================================
void GatewayImpl::Release() {
    delete this;
}

void GatewayImpl::RegisterHandler(IGatewayHandler* handler) {
    handler_ = handler;
}

void GatewayImpl::SetThreadPool(ThreadPool* pool) {
    threadPool_ = pool;
}

void GatewayImpl::SetMaxConnections(int max) {
    maxConnections_ = max;
}

void GatewayImpl::SetIdleTimeout(int seconds) {
    idleTimeoutSec_ = seconds;
}

bool GatewayImpl::Start(const char* bindAddr, int port) {
    if (routerSock_) {
        std::cerr << "Gateway already started\n";
        return false;
    }

    // 1. 创建 ROUTER socket
    void* router = CreateRouterSocket();
    if (!router) {
        std::cerr << "Failed to create ZMQ_ROUTER socket\n";
        return false;
    }

    // 启用 ZMQ 内置心跳：每隔 3s 发送一次，9s 未收到则判定断开
    SetZmqHeartbeat(router, 3000, 9000, 20000);

    std::string routerAddr = "tcp://" + std::string(bindAddr) + ":" + std::to_string(port);
    if (!BindSocket(router, routerAddr.c_str())) {
        CloseSocket(router);
        return false;
    }

    // 2. 创建 PUB socket（广播端口 = router 端口 + 1）
    void* pub = CreatePubSocket();
    if (!pub) {
        std::cerr << "Failed to create ZMQ_PUB socket\n";
        CloseSocket(router);
        return false;
    }

    std::string pubAddr = "tcp://" + std::string(bindAddr) + ":" + std::to_string(port + 1);
    if (!BindSocket(pub, pubAddr.c_str())) {
        CloseSocket(pub);
        CloseSocket(router);
        return false;
    }

    routerSock_ = router;
    pubSock_ = pub;
    bindAddr_ = bindAddr;
    routerPort_ = port;

    std::cout << "Gateway (ZMQ_ROUTER) listening on " << routerAddr << "\n";
    std::cout << "Gateway (ZMQ_PUB)    listening on " << pubAddr << "\n";

    running_ = true;
    dispatchThread_ = std::thread(&GatewayImpl::DispatchThreadFunc, this);
    return true;
}

void GatewayImpl::Stop() {
    running_ = false;

    // 先等待 dispatch 线程退出（zmq_poll 100ms 超时，最多 100ms 内响应 running_）
    if (dispatchThread_.joinable()) {
        dispatchThread_.join();
    }

    // 再安全关闭 socket
    if (routerSock_) {
        CloseSocket(routerSock_);
        routerSock_ = nullptr;
    }
    if (pubSock_) {
        CloseSocket(pubSock_);
        pubSock_ = nullptr;
    }
}

// ============================================================================
// Dispatch 线程（单线程接收所有客户端消息 → 提交 ThreadPool）
// ============================================================================
void GatewayImpl::DispatchThreadFunc() {
    // 已知 client identity 集合（用于连接数控制）
    std::set<std::vector<uint8_t>> knownIdentities;

    zmq_pollitem_t items[] = { { routerSock_, 0, ZMQ_POLLIN, 0 } };

    while (running_) {
        // 100ms 超时轮询，确保 Stop() 后能及时退出
        int rc = zmq_poll(items, 1, 100);
        if (rc < 0) {
            if (running_) std::cerr << "zmq_poll error: " << zmq_strerror(zmq_errno()) << "\n";
            break;
        }
        if (rc == 0) {
            // 超时：检查空闲连接
            if (idleTimeoutSec_ > 0) {
                CleanupIdleConnections();
            }
            continue;
        }

        if (!(items[0].revents & ZMQ_POLLIN)) continue;

        std::vector<uint8_t> identity;
        uint16_t rawType = 0;
        std::vector<uint8_t> body;

        if (!RecvRouterMessage(routerSock_, identity, rawType, body)) {
            if (running_) continue;
            break;
        }

        // 更新活跃时间
        {
            std::lock_guard<std::mutex> lock(connMutex_);
            lastActive_[identity] = std::chrono::steady_clock::now();
        }

        auto msgType = static_cast<GatewayMsgType>(rawType);

        // 心跳消息直接在 dispatch 线程处理，避免线程池延迟
        if (msgType == GatewayMsgType::kPing) {
            SendPong(identity);
            continue;
        }

        // 连接数控制：新 identity 时检查
        if (maxConnections_ > 0) {
            bool isNew = (knownIdentities.find(identity) == knownIdentities.end());
            if (isNew) {
                int cur = activeConnections_.load();
                if (cur >= maxConnections_) {
                    // 拒绝新连接：发送错误响应
                    gateway_proto::LoginResponse rsp;
                    rsp.set_error_code(3);  // server busy
                    rsp.set_error_msg("Server busy, max connections reached");
                    std::string data = rsp.SerializeAsString();
                    std::vector<uint8_t> respBody(data.begin(), data.end());
                    SendRouterMessage(routerSock_, identity,
                                      static_cast<uint16_t>(GatewayMsgType::kLoginResponse),
                                      respBody);
                    continue;
                }
                knownIdentities.insert(identity);
                activeConnections_++;
                std::cout << "New client connected (active=" << activeConnections_.load() << ")\n";
            }
        }

        // 提交到线程池处理（若未设置线程池则同步处理）
        if (threadPool_) {
            auto idCopy = std::make_shared<std::vector<uint8_t>>(identity);
            auto bodyCopy = std::make_shared<std::vector<uint8_t>>(body);
            threadPool_->Submit([this, idCopy, rawType, bodyCopy] {
                ProcessMessage(*idCopy, rawType, *bodyCopy);
            });
        } else {
            ProcessMessage(identity, rawType, body);
        }
    }
}

// ============================================================================
// 消息处理（解析 → 分发 handler → 回复发送）
// ============================================================================
void GatewayImpl::ProcessMessage(const std::vector<uint8_t>& identity,
                                  uint16_t rawType, const std::vector<uint8_t>& body) {
    // 若正在关闭则丢弃消息，避免在已关闭的 socket 上发送
    if (!running_) return;

    auto msgType = static_cast<GatewayMsgType>(rawType);

    switch (msgType) {
    case GatewayMsgType::kLoginRequest: {
        gateway_proto::LoginRequest req;
        if (!req.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            std::cerr << "Failed to parse LoginRequest\n";
            return;
        }

        gateway_proto::LoginResponse rsp;
        if (handler_ && handler_->OnLogin(req, rsp)) {
            std::string data = rsp.SerializeAsString();
            std::vector<uint8_t> respBody(data.begin(), data.end());
            SendRouterMessage(routerSock_, identity,
                              static_cast<uint16_t>(GatewayMsgType::kLoginResponse),
                              respBody);
        }
        break;
    }
    default:
        std::cerr << "Unknown message type: " << rawType << "\n";
        break;
    }
}

void GatewayImpl::SendPong(const std::vector<uint8_t>& identity) {
    std::vector<uint8_t> emptyBody;
    if (!SendRouterMessage(routerSock_, identity,
                           static_cast<uint16_t>(GatewayMsgType::kPong), emptyBody)) {
        std::cerr << "Failed to send Pong to client\n";
    }
}

void GatewayImpl::CleanupIdleConnections() {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto now = std::chrono::steady_clock::now();
    for (auto it = lastActive_.begin(); it != lastActive_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
        if (elapsed >= idleTimeoutSec_) {
            std::cout << "Client idle timeout, removing identity ("
                      << elapsed << "s >= " << idleTimeoutSec_ << "s)\n";
            it = lastActive_.erase(it);
            int remaining = activeConnections_.fetch_sub(1) - 1;
            if (remaining < 0) activeConnections_.store(0);
        } else {
            ++it;
        }
    }
}

void GatewayImpl::BroadcastNotification(const std::string& content) {
    if (!pubSock_) {
        std::cerr << "PUB socket not ready, broadcast skipped\n";
        return;
    }

    // ZMQ PUB/SUB slow-joiner 缓解策略：
    // SUB socket 从 connect → subscribe 到过滤器同步至 PUB 需要短暂时间（~100ms），
    // 期间发送的消息会被丢弃。通过重复发送 + 间隔等待确保新客户端能收到广播。
    // 所有重发使用相同的 message_id，客户端根据 message_id 去重，不会重复回调。
    static constexpr int kBroadcastRetryCount = 3;
    static constexpr int kBroadcastRetryIntervalMs = 100;
    static std::atomic<uint64_t> sBroadcastSeq{0};

    uint64_t msgId = sBroadcastSeq.fetch_add(1) + 1;

    gateway_proto::BroadcastNotification notif;
    notif.set_content(content);
    notif.set_message_id(msgId);
    notif.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
    );

    std::string data = notif.SerializeAsString();
    std::vector<uint8_t> body(data.begin(), data.end());

    bool ok = false;
    for (int i = 0; i < kBroadcastRetryCount; ++i) {
        if (SendMessage(pubSock_, static_cast<uint16_t>(GatewayMsgType::kBroadcastNotification), body)) {
            ok = true;
        }
        if (i < kBroadcastRetryCount - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kBroadcastRetryIntervalMs));
        }
    }

    if (ok) {
        std::cout << "Broadcast sent to all clients (msg_id=" << msgId
                  << "): " << content << "\n";
    } else {
        std::cerr << "Failed to broadcast notification\n";
    }
}

} // namespace gateway
