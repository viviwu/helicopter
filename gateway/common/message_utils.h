/**
  ******************************************************************************
  * @file           : message_utils.h
  * @author         : vivi wu
  * @brief          : 长度前缀 + 消息类型 的 TCP 收发包工具
  * @version        : 0.2.0
  * @date           : 09/05/26
  ******************************************************************************
  */

#ifndef PROTO_GATEWAY_MESSAGE_UTILS_H
#define PROTO_GATEWAY_MESSAGE_UTILS_H

#pragma once

#include <cstdint>
#include <vector>
#include <string>

// 从 socket 读取一条完整的长度前缀消息
// 消息格式: [4字节大端body长度] + [2字节大端msg_type] + [body]
// 成功返回 true，失败返回 false
bool recvMessage(int sockfd, uint16_t& outMsgType, std::vector<uint8_t>& outBody);

// 发送一条长度前缀消息
bool sendMessage(int sockfd, uint16_t msgType, const std::vector<uint8_t>& body);

#endif //PROTO_GATEWAY_MESSAGE_UTILS_H
