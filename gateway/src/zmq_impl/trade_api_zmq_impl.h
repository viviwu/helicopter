/**
 ******************************************************************************
 * @file           : trade_api_zmq_impl.h
 * @author         : vivi wu
 * @brief          : TradeApi 底层 ZMQ DEALER 基础设施（recv loop + 心跳）
 * @version        : 0.1.0
 * @date           : 13/05/26
 ******************************************************************************
 */
#ifndef TRADE_API_ZMQ_IMPL_H
#define TRADE_API_ZMQ_IMPL_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace trade {

class TradeApiZmqImpl {
public:
    TradeApiZmqImpl() = default;
    ~TradeApiZmqImpl();

    TradeApiZmqImpl(const TradeApiZmqImpl&) = delete;
    TradeApiZmqImpl& operator=(const TradeApiZmqImpl&) = delete;

    int Connect(const char* address, int routerPort, int heartbeatSec);
    void Disconnect();
    bool IsConnected() const { return connected_.load(); }
    bool Send(uint16_t msgType, const std::vector<uint8_t>& body);

    std::function<void()> on_connected;
    std::function<void(int reason)> on_disconnected;
    std::function<void(uint16_t msgType, const std::vector<uint8_t>& body)> on_message;

private:
    void RecvLoop();
    void HeartbeatLoop();

    std::string address_;
    int routerPort_ = 0;
    int heartbeatSec_ = 5;
    void* dealerSock_ = nullptr;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread recvThread_;
    std::thread heartbeatThread_;

    using TimePoint = std::chrono::steady_clock::time_point;
    std::atomic<TimePoint> lastPongTime_{TimePoint{}};
    std::atomic<int> missedPongs_{0};
    static constexpr int kMaxMissedPongs = 3;
};

} // namespace trade

#endif // TRADE_API_ZMQ_IMPL_H
