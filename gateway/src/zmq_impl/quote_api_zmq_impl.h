/**
 ******************************************************************************
 * @file           : quote_api_zmq_impl.h
 * @author         : vivi wu
 * @brief          : QuoteApi 底层 ZMQ SUB 基础设施（recv loop + 主题订阅）
 * @version        : 0.1.0
 * @date           : 13/05/26
 ******************************************************************************
 */
#ifndef QUOTE_API_ZMQ_IMPL_H
#define QUOTE_API_ZMQ_IMPL_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace quote {

class QuoteApiZmqImpl {
public:
    QuoteApiZmqImpl() = default;
    ~QuoteApiZmqImpl();

    QuoteApiZmqImpl(const QuoteApiZmqImpl&) = delete;
    QuoteApiZmqImpl& operator=(const QuoteApiZmqImpl&) = delete;

    int Connect(const char* address, int pubPort);
    void Disconnect();
    bool IsConnected() const { return connected_.load(); }
    bool Subscribe(const std::string& topic);
    bool Unsubscribe(const std::string& topic);

    std::function<void()> on_connected;
    std::function<void(int reason)> on_disconnected;
    std::function<void(const std::string& topic, uint16_t msgType,
                       const std::vector<uint8_t>& body)> on_pub_message;

private:
    void RecvLoop();

    std::string address_;
    int pubPort_ = 0;
    void* subSock_ = nullptr;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread recvThread_;
};

} // namespace quote

#endif // QUOTE_API_ZMQ_IMPL_H
