/**
  ******************************************************************************
  * @file           : message_utils.h
  * @author         : vivi wu
  * @brief          : ZeroMQ 多部分消息收发包工具
  * @version        : 0.3.0
  * @date           : 09/05/26
  ******************************************************************************
  */

#ifndef PROTO_GATEWAY_MESSAGE_UTILS_H
#define PROTO_GATEWAY_MESSAGE_UTILS_H

#pragma once

#include <cstdint>
#include <vector>

// ============================================================================
// ZeroMQ Context（进程级单例）
// ============================================================================

/// 获取进程共享的 ZMQ context（首次调用时创建，进程退出时自动销毁）
void* GetZmqContext();

// ============================================================================
// Socket 生命周期
// ============================================================================

/// 创建 ZMQ_ROUTER socket（服务端），失败返回 nullptr
void* CreateRouterSocket();

/// 创建 ZMQ_DEALER socket（客户端），失败返回 nullptr
void* CreateDealerSocket();

/// 创建 ZMQ_PUB socket（服务端广播），失败返回 nullptr
void* CreatePubSocket();

/// 创建 ZMQ_SUB socket（客户端订阅），失败返回 nullptr
void* CreateSubSocket();

/// 关闭 socket（自动设置 linger=0 确保快速关闭）
void CloseSocket(void* sock);

// ============================================================================
// Socket 选项
// ============================================================================

/// 设置 socket linger 时间（ms），0 表示立即关闭不等待
void SetLinger(void* sock, int ms);

/// 设置接收超时（ms），用于空闲连接回收
void SetRecvTimeout(void* sock, int ms);

/// 启用 TCP KeepAlive 并设置参数
void SetTcpKeepalive(void* sock, int keepalive, int idle, int interval, int count);

/// 启用 ZMQ 内置心跳（ZMQ_HEARTBEAT_IVL / ZMQ_HEARTBEAT_TIMEOUT / ZMQ_HEARTBEAT_TTL）
/// @param ivlMs      心跳发送间隔（毫秒），0 表示禁用
/// @param timeoutMs  心跳超时（毫秒），超过此时间未收到对端心跳则视为断开
/// @param ttlMs      心跳消息 TTL（毫秒），一般设为 timeoutMs 的 2~3 倍
void SetZmqHeartbeat(void* sock, int ivlMs, int timeoutMs, int ttlMs);

/// 设置 SUB socket 订阅过滤器（空字符串 "" 表示接收所有消息）
void SetSubSubscribe(void* sock, const char* filter);

// ============================================================================
// 绑定 / 连接
// ============================================================================

/// 绑定到地址，如 "tcp://0.0.0.0:12345"
bool BindSocket(void* sock, const char* address);

/// 连接到地址，如 "tcp://127.0.0.1:12345"
bool ConnectSocket(void* sock, const char* address);

// ============================================================================
// 消息收发（DEALER 端 — 客户端使用）
// ============================================================================

/// 从 DEALER socket 接收一条消息（2 帧：msg_type + body）
bool RecvMessage(void* sock, uint16_t& outMsgType, std::vector<uint8_t>& outBody);

/// 通过 DEALER socket 发送一条消息（2 帧：msg_type + body）
bool SendMessage(void* sock, uint16_t msgType, const std::vector<uint8_t>& body);

// ============================================================================
// 消息收发（ROUTER 端 — 服务端使用，额外携带 identity 帧）
// ============================================================================

/// 从 ROUTER socket 接收一条消息（3 帧：identity + msg_type + body）
bool RecvRouterMessage(void* sock, std::vector<uint8_t>& outIdentity,
                       uint16_t& outMsgType, std::vector<uint8_t>& outBody);

/// 通过 ROUTER socket 向指定 identity 发送回复（3 帧：identity + msg_type + body）
bool SendRouterMessage(void* sock, const std::vector<uint8_t>& identity,
                       uint16_t msgType, const std::vector<uint8_t>& body);

#endif //PROTO_GATEWAY_MESSAGE_UTILS_H
