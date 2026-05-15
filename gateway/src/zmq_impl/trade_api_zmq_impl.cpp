/**
 ******************************************************************************
 * @file           : trade_api_zmq_impl.cpp
 * @author         : vivi wu
 * @brief          : TradeApi 底层 ZMQ DEALER 基础设施实现
 * @version        : 0.1.0
 * @date           : 13/05/26
 ******************************************************************************
 */
#include "trade_api_zmq_impl.h"
#include "common/message_utils.h"

#include <iostream>

#include <zmq.h>

namespace trade {

TradeApiZmqImpl::~TradeApiZmqImpl() {
    Disconnect();
}

int TradeApiZmqImpl::Connect(const char* address, int routerPort, int heartbeatSec) {
    if (dealerSock_) {
        return 0;
    }

    if (routerPort <= 0) {
        std::cerr << "TradeApiZmqImpl: routerPort must be > 0\n";
        return -1;
    }

    address_ = address ? address : "";
    routerPort_ = routerPort;
    heartbeatSec_ = heartbeatSec;

    void* dealer = CreateDealerSocket();
    if (!dealer) {
        std::cerr << "TradeApiZmqImpl: failed to create ZMQ_DEALER socket\n";
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
    connected_ = true;
    running_ = true;
    lastPongTime_ = std::chrono::steady_clock::now();
    missedPongs_ = 0;

    recvThread_ = std::thread(&TradeApiZmqImpl::RecvLoop, this);
    heartbeatThread_ = std::thread(&TradeApiZmqImpl::HeartbeatLoop, this);

    if (on_connected) on_connected();
    return 0;
}

void TradeApiZmqImpl::Disconnect() {
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

    connected_ = false;
}

bool TradeApiZmqImpl::Send(uint16_t msgType, const std::vector<uint8_t>& body) {
    if (!dealerSock_) return false;
    return SendMessage(dealerSock_, msgType, body);
}

void TradeApiZmqImpl::RecvLoop() {
    zmq_pollitem_t item;
    item.socket = dealerSock_;
    item.fd = 0;
    item.events = ZMQ_POLLIN;

    while (running_.load()) {
        int rc = zmq_poll(&item, 1, 100);
        if (rc < 0) {
            if (running_.load())
                std::cerr << "TradeApiZmqImpl: zmq_poll error: " << zmq_strerror(zmq_errno()) << "\n";
            break;
        }
        if (rc == 0) continue;

        if (item.revents & ZMQ_POLLIN) {
            uint16_t rawType = 0;
            std::vector<uint8_t> body;
            if (!RecvMessage(dealerSock_, rawType, body)) {
                if (running_.load()) {
                    std::cerr << "TradeApiZmqImpl: DEALER recv error, disconnecting\n";
                }
                running_ = false;
                if (on_disconnected) on_disconnected(0);
                break;
            }

            static constexpr uint16_t kPongType = 4;
            if (rawType == kPongType) {
                lastPongTime_ = std::chrono::steady_clock::now();
                missedPongs_ = 0;
                continue;
            }

            if (on_message) on_message(rawType, body);
        }
    }
}

void TradeApiZmqImpl::HeartbeatLoop() {
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
            std::cerr << "TradeApiZmqImpl: failed to send Ping\n";
            int missed = missedPongs_.fetch_add(1) + 1;
            if (missed >= kMaxMissedPongs) {
                running_ = false;
                if (on_disconnected) on_disconnected(1);
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
            std::cerr << "TradeApiZmqImpl: heartbeat timeout (no Pong for " << elapsedSec << "s)\n";
            running_ = false;
            if (on_disconnected) on_disconnected(1);
            break;
        }
    }
}

} // namespace trade
