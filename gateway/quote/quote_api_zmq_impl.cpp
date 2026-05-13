/**
 ******************************************************************************
 * @file           : quote_api_zmq_impl.cpp
 * @author         : vivi wu
 * @brief          : QuoteApi 底层 ZMQ SUB 基础设施实现
 * @version        : 0.1.0
 * @date           : 13/05/26
 ******************************************************************************
 */
#include "quote/quote_api_zmq_impl.h"
#include "common/message_utils.h"

#include <iostream>

#include <zmq.h>

namespace quote {

QuoteApiZmqImpl::~QuoteApiZmqImpl() {
    Disconnect();
}

int QuoteApiZmqImpl::Connect(const char* address, int pubPort) {
    if (subSock_) {
        return 0;
    }

    if (pubPort <= 0) {
        std::cerr << "QuoteApiZmqImpl: pubPort must be > 0\n";
        return -1;
    }

    address_ = address ? address : "";
    pubPort_ = pubPort;

    void* sub = CreateSubSocket();
    if (!sub) {
        std::cerr << "QuoteApiZmqImpl: failed to create ZMQ_SUB socket\n";
        return -1;
    }

    SetSubSubscribe(sub, "");

    std::string subAddr = "tcp://" + address_ + ":" + std::to_string(pubPort_);
    if (!ConnectSocket(sub, subAddr.c_str())) {
        std::cerr << "QuoteApiZmqImpl: failed to connect SUB socket to " << subAddr << "\n";
        CloseSocket(sub);
        return -1;
    }

    subSock_ = sub;
    connected_ = true;
    running_ = true;

    recvThread_ = std::thread(&QuoteApiZmqImpl::RecvLoop, this);

    if (on_connected) on_connected();
    return 0;
}

void QuoteApiZmqImpl::Disconnect() {
    running_ = false;

    if (recvThread_.joinable()) {
        recvThread_.join();
    }

    if (subSock_) {
        CloseSocket(subSock_);
        subSock_ = nullptr;
    }

    connected_ = false;
}

bool QuoteApiZmqImpl::Subscribe(const std::string& topic) {
    if (!subSock_) return false;
    SetSubSubscribe(subSock_, topic.c_str());
    return true;
}

bool QuoteApiZmqImpl::Unsubscribe(const std::string& topic) {
    if (!subSock_) return false;
    zmq_setsockopt(subSock_, ZMQ_UNSUBSCRIBE, topic.c_str(), topic.size());
    return true;
}

void QuoteApiZmqImpl::RecvLoop() {
    zmq_pollitem_t item;
    item.socket = subSock_;
    item.fd = 0;
    item.events = ZMQ_POLLIN;

    while (running_.load()) {
        int rc = zmq_poll(&item, 1, 100);
        if (rc < 0) {
            if (running_.load())
                std::cerr << "QuoteApiZmqImpl: zmq_poll error: " << zmq_strerror(zmq_errno()) << "\n";
            break;
        }
        if (rc == 0) continue;

        if (item.revents & ZMQ_POLLIN) {
            std::string topic;
            uint16_t rawType = 0;
            std::vector<uint8_t> body;
            if (!RecvPubMessage(subSock_, topic, rawType, body)) {
                if (running_.load()) {
                    std::cerr << "QuoteApiZmqImpl: SUB recv error\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            if (on_pub_message) on_pub_message(topic, rawType, body);
        }
    }
}

} // namespace quote
