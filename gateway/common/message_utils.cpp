/**
  ******************************************************************************
  * @file           : message_utils.cpp
  * @author         : vivi wu
  * @brief          : 长度前缀 + 消息类型 的 TCP 收发包实现
  * @version        : 0.2.0
  * @date           : 09/05/26
  ******************************************************************************
  */

#include "message_utils.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

static bool readFull(int sockfd, void* buf, size_t len) {
    size_t total = 0;
    auto* ptr = static_cast<uint8_t*>(buf);
    while (total < len) {
        ssize_t n = recv(sockfd, ptr + total, len - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

bool recvMessage(int sockfd, uint16_t& outMsgType, std::vector<uint8_t>& outBody) {
    // 1. 读取4字节长度
    uint32_t netLen = 0;
    if (!readFull(sockfd, &netLen, sizeof(netLen)))
        return false;
    uint32_t bodyLen = ntohl(netLen);

    // 2. 读取2字节消息类型
    uint16_t netType = 0;
    if (!readFull(sockfd, &netType, sizeof(netType)))
        return false;
    outMsgType = ntohs(netType);

    // 3. 读取 body
    outBody.resize(bodyLen);
    if (!readFull(sockfd, outBody.data(), bodyLen))
        return false;

    return true;
}

bool sendMessage(int sockfd, uint16_t msgType, const std::vector<uint8_t>& body) {
    // 发送4字节长度（大端）
    uint32_t netLen = htonl(static_cast<uint32_t>(body.size()));
    if (send(sockfd, &netLen, sizeof(netLen), 0) != sizeof(netLen))
        return false;

    // 发送2字节消息类型（大端）
    uint16_t netType = htons(msgType);
    if (send(sockfd, &netType, sizeof(netType), 0) != sizeof(netType))
        return false;

    // 发送 body
    size_t total = 0;
    while (total < body.size()) {
        ssize_t n = send(sockfd, body.data() + total, body.size() - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}
