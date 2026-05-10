/**
  ******************************************************************************
  * @file           : GatewayApi.cpp
  * @author         : vivi wu
  * @brief          : DEALER+SUB 客户端基类实现
  * @version        : 0.2.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#include "framework/GatewayApi.h"
#include "common/message_utils.h"

#include <iostream>

#include <zmq.h>

namespace framework {

GatewayApi::~GatewayApi() {
    Disconnect();
}

// ============================================================================
// 生命周期
// ============================================================================

int GatewayApi::Connect(const char* address, int routerPort,
                            int pubPort, int heartbeatSec) {
    if (dealerSock_ || subSock_) {
        return 0;  // already connected
    }

    if (routerPort <= 0 && pubPort <= 0) {
        std::cerr << "At least one of routerPort or pubPort must be > 0\n";
        return -1;
    }

    address_ = address ? address : "";
    routerPort_ = routerPort;
    pubPort_ = pubPort;
    heartbeatSec_ = heartbeatSec;

    // 1. DEALER socket（可选）
    if (routerPort_ > 0) {
        void* dealer = CreateDealerSocket();
        if (!dealer) {
            std::cerr << "Failed to create ZMQ_DEALER socket\n";
            return -1;
        }

        SetZmqHeartbeat(dealer, 3000, 9000, 20000);
        SetTcpKeepalive(dealer, 1, heartbeatSec_, heartbeatSec_, 3);

        std::string dealerAddr = "tcp://" + address_ + ":" + std::to_string(routerPort_);
        if (!ConnectSocket(dealer, dealerAddr.c_str())) {
            CloseSocket(dealer);
            return -1;
        }

        dealerSock_ = dealer;
    }

    // 2. SUB socket（可选）
    if (pubPort_ > 0) {
        void* sub = CreateSubSocket();
        if (!sub) {
            std::cerr << "Failed to create ZMQ_SUB socket\n";
            if (dealerSock_) { CloseSocket(dealerSock_); dealerSock_ = nullptr; }
            return -1;
        }

        // 默认订阅空字符串（接收所有 topic 消息）
        SetSubSubscribe(sub, "");

        std::string subAddr = "tcp://" + address_ + ":" + std::to_string(pubPort_);
        if (!ConnectSocket(sub, subAddr.c_str())) {
            std::cerr << "Failed to connect SUB socket to " << subAddr << "\n";
            CloseSocket(sub);
            if (dealerSock_) { CloseSocket(dealerSock_); dealerSock_ = nullptr; }
            return -1;
        }

        subSock_ = sub;
    }

    connected_ = true;
    running_ = true;
    lastPongTime_ = std::chrono::steady_clock::now();
    missedPongs_ = 0;

    recvThread_ = std::thread(&GatewayApi::RecvLoop, this);

    // 心跳仅在有 DEALER 时启动（SUB 是被动接收，无需心跳）
    if (dealerSock_) {
        heartbeatThread_ = std::thread(&GatewayApi::HeartbeatLoop, this);
    }

    OnConnected();
    return 0;
}

void GatewayApi::Disconnect() {
    running_ = false;

    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
    if (recvThread_.joinable()) {
        recvThread_.join();
    }

    if (dealerSock_) {
        CloseSocket(dealerSock_);
        dealerSock_ = nullptr;
    }
    if (subSock_) {
        CloseSocket(subSock_);
        subSock_ = nullptr;
    }

    connected_ = false;
}

// ============================================================================
// SUB 主题管理
// ============================================================================

bool GatewayApi::Subscribe(const std::string& topic) {
    if (!subSock_) return false;
    SetSubSubscribe(subSock_, topic.c_str());
    return true;
}

bool GatewayApi::Unsubscribe(const std::string& topic) {
    if (!subSock_) return false;
    zmq_setsockopt(subSock_, ZMQ_UNSUBSCRIBE, topic.c_str(), topic.size());
    return true;
}

// ============================================================================
// 发送
// ============================================================================

bool GatewayApi::SendToRouter(uint16_t msgType, const std::vector<uint8_t>& body) {
    if (!dealerSock_) return false;
    return SendMessage(dealerSock_, msgType, body);
}

// ============================================================================
// 接收循环
// ============================================================================

void GatewayApi::RecvLoop() {
    zmq_pollitem_t items[2];
    int itemCount = 0;
    int dealerIdx = -1;
    int subIdx = -1;

    if (dealerSock_) {
        dealerIdx = itemCount;
        items[itemCount].socket = dealerSock_;
        items[itemCount].fd = 0;
        items[itemCount].events = ZMQ_POLLIN;
        itemCount++;
    }

    if (subSock_) {
        subIdx = itemCount;
        items[itemCount].socket = subSock_;
        items[itemCount].fd = 0;
        items[itemCount].events = ZMQ_POLLIN;
        itemCount++;
    }

    while (running_.load()) {
        int rc = zmq_poll(items, itemCount, 100);
        if (rc < 0) {
            if (running_.load())
                std::cerr << "zmq_poll error: " << zmq_strerror(zmq_errno()) << "\n";
            break;
        }
        if (rc == 0) continue;

        // ---- DEALER socket ----
        if (dealerIdx >= 0 && (items[dealerIdx].revents & ZMQ_POLLIN)) {
            uint16_t rawType = 0;
            std::vector<uint8_t> body;
            if (!RecvMessage(dealerSock_, rawType, body)) {
                if (running_.load()) {
                    std::cerr << "DEALER recv error, disconnecting\n";
                }
                Disconnect();
                OnDisconnected(0);
                break;
            }

            static constexpr uint16_t kPongType = 4;
            if (rawType == kPongType) {
                lastPongTime_ = std::chrono::steady_clock::now();
                missedPongs_ = 0;
                continue;
            }

            OnRouterMessage(rawType, body);
        }

        // ---- SUB socket ----
        if (subIdx >= 0 && (items[subIdx].revents & ZMQ_POLLIN)) {
            std::string topic;
            uint16_t rawType = 0;
            std::vector<uint8_t> body;
            if (!RecvPubMessage(subSock_, topic, rawType, body)) {
                if (running_.load()) {
                    std::cerr << "SUB recv error\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            OnPubMessage(topic, rawType, body);
        }
    }
}

// ============================================================================
// 心跳循环
// ============================================================================

void GatewayApi::HeartbeatLoop() {
    static constexpr uint16_t kPingType = 3;

    while (running_.load()) {
        int slept = 0;
        int targetMs = heartbeatSec_ * 1000;
        while (slept < targetMs && running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            slept += 100;
        }
        if (!running_.load()) break;

        std::vector<uint8_t> emptyBody;
        if (!SendMessage(dealerSock_, kPingType, emptyBody)) {
            std::cerr << "Failed to send Ping\n";
            int missed = missedPongs_.fetch_add(1) + 1;
            if (missed >= kMaxMissedPongs) {
                running_ = false;
                OnDisconnected(1);  // heartbeat timeout
                break;
            }
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto lastPong = lastPongTime_.load();
        auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(
            now - lastPong).count();
        int threshold = heartbeatSec_ * kMaxMissedPongs;
        if (elapsedSec >= threshold) {
            std::cerr << "Heartbeat timeout (no Pong for " << elapsedSec << "s)\n";
            running_ = false;
            OnDisconnected(1);
            break;
        }
    }
}

} // namespace framework
