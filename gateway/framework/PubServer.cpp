/**
  ******************************************************************************
  * @file           : PubServer.cpp
  * @author         : vivi wu
  * @brief          : PUB 模式发布端基类实现
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#include "framework/PubServer.h"
#include "common/message_utils.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace framework {

PubServer::~PubServer() {
    Stop();
}

bool PubServer::Start(const char* bindAddr, int port) {
    if (pubSock_) {
        std::cerr << "PubServer already started\n";
        return false;
    }

    void* pub = CreatePubSocket();
    if (!pub) {
        std::cerr << "Failed to create ZMQ_PUB socket\n";
        return false;
    }

    std::string addr = "tcp://" + std::string(bindAddr) + ":" + std::to_string(port);
    if (!BindSocket(pub, addr.c_str())) {
        CloseSocket(pub);
        return false;
    }

    pubSock_ = pub;
    std::cout << "[PubServer] listening on " << addr << "\n";
    return true;
}

void PubServer::Stop() {
    if (pubSock_) {
        CloseSocket(pubSock_);
        pubSock_ = nullptr;
    }
}

bool PubServer::Publish(const std::string& topic, uint16_t msgType,
                         const std::vector<uint8_t>& body) {
    if (!pubSock_) {
        std::cerr << "PubServer not started\n";
        return false;
    }

    // Slow-joiner 缓解：重发 3 次，间隔 100ms
    static constexpr int kRetryCount = 3;
    static constexpr int kRetryIntervalMs = 100;

    bool ok = false;
    for (int i = 0; i < kRetryCount; ++i) {
        if (SendPubMessage(pubSock_, topic, msgType, body)) {
            ok = true;
        }
        if (i < kRetryCount - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kRetryIntervalMs));
        }
    }

    return ok;
}

} // namespace framework
