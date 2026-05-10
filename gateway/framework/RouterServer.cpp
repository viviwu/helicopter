/**
  ******************************************************************************
  * @file           : RouterServer.cpp
  * @author         : vivi wu
  * @brief          : ROUTER 模式服务端基类实现
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#include "framework/RouterServer.h"
#include "common/message_utils.h"
#include "common/ThreadPool.h"

#include <iostream>
#include <set>

#include <zmq.h>

namespace framework {

RouterServer::~RouterServer() {
    Stop();
}

// ============================================================================
// 生命周期
// ============================================================================

bool RouterServer::Start(const char* bindAddr, int port) {
    if (routerSock_) {
        std::cerr << "RouterServer already started\n";
        return false;
    }

    void* router = CreateRouterSocket();
    if (!router) {
        std::cerr << "Failed to create ZMQ_ROUTER socket\n";
        return false;
    }

    SetZmqHeartbeat(router, 3000, 9000, 20000);

    std::string addr = "tcp://" + std::string(bindAddr) + ":" + std::to_string(port);
    if (!BindSocket(router, addr.c_str())) {
        CloseSocket(router);
        return false;
    }

    routerSock_ = router;

    std::cout << "[RouterServer] listening on " << addr << "\n";

    running_ = true;
    dispatchThread_ = std::thread(&RouterServer::DispatchLoop, this);
    return true;
}

void RouterServer::Stop() {
    running_ = false;

    if (dispatchThread_.joinable()) {
        dispatchThread_.join();
    }

    if (routerSock_) {
        CloseSocket(routerSock_);
        routerSock_ = nullptr;
    }
}

// ============================================================================
// 消息回复
// ============================================================================

bool RouterServer::SendReply(const std::vector<uint8_t>& identity,
                              uint16_t msgType, const std::vector<uint8_t>& body) {
    if (!routerSock_) return false;
    return SendRouterMessage(routerSock_, identity, msgType, body);
}

// ============================================================================
// Dispatch 循环
// ============================================================================

void RouterServer::DispatchLoop() {
    std::set<std::vector<uint8_t>> knownIdentities;

    zmq_pollitem_t items[] = { { routerSock_, 0, ZMQ_POLLIN, 0 } };

    while (running_) {
        int rc = zmq_poll(items, 1, 100);
        if (rc < 0) {
            if (running_) std::cerr << "zmq_poll error: " << zmq_strerror(zmq_errno()) << "\n";
            break;
        }
        if (rc == 0) {
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

        // kPing 和 kPong 的枚举值在 gateway_msg_types.h 中定义（3 和 4）
        // 使用数值常量避免框架层依赖业务枚举
        static constexpr uint16_t kPingType = 3;
        static constexpr uint16_t kPongType = 4;

        if (rawType == kPingType) {
            HandlePing(identity);
            continue;
        }

        // 连接数控制
        if (maxConnections_ > 0) {
            bool isNew = (knownIdentities.find(identity) == knownIdentities.end());
            if (isNew) {
                int cur = activeConnections_.load();
                if (cur >= maxConnections_) {
                    // 拒绝：发送空回复，error 由业务层处理
                    std::cerr << "Max connections reached, rejecting new client\n";
                    continue;
                }
                knownIdentities.insert(identity);
                activeConnections_++;
                std::cout << "[RouterServer] new client (active="
                          << activeConnections_.load() << ")\n";
            }
        }

        // 提交到线程池或同步处理
        if (threadPool_) {
            auto idCopy = std::make_shared<std::vector<uint8_t>>(identity);
            auto bodyCopy = std::make_shared<std::vector<uint8_t>>(body);
            threadPool_->Submit([this, idCopy, rawType, bodyCopy] {
                OnMessage(*idCopy, rawType, *bodyCopy);
            });
        } else {
            OnMessage(identity, rawType, body);
        }
    }
}

void RouterServer::HandlePing(const std::vector<uint8_t>& identity) {
    static constexpr uint16_t kPongType = 4;
    std::vector<uint8_t> emptyBody;
    SendRouterMessage(routerSock_, identity, kPongType, emptyBody);
}

void RouterServer::CleanupIdleConnections() {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto now = std::chrono::steady_clock::now();
    for (auto it = lastActive_.begin(); it != lastActive_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second).count();
        if (elapsed >= idleTimeoutSec_) {
            std::cout << "[RouterServer] idle timeout (" << elapsed << "s), removing client\n";
            it = lastActive_.erase(it);
            int remaining = activeConnections_.fetch_sub(1) - 1;
            if (remaining < 0) activeConnections_.store(0);
        } else {
            ++it;
        }
    }
}

} // namespace framework
