/**
  ******************************************************************************
  * @file           : PubServer.h
  * @author         : vivi wu
  * @brief          : PUB 模式发布端基类（ZMQ_PUB / 主题广播）
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef FRAMEWORK_PUB_SERVER_H
#define FRAMEWORK_PUB_SERVER_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace framework {

/// PUB 模式发布端基类
/// 管理 ZMQ_PUB socket，支持按 topic 发布消息。
/// 内建 slow-joiner 缓解：每条消息重发 3 次（间隔 100ms），
/// 使用 message_id 帮助客户端去重。
class PubServer {
public:
    PubServer() = default;
    virtual ~PubServer();

    // ==========================================================================
    // 生命周期
    // ==========================================================================

    bool Start(const char* bindAddr, int port);
    void Stop();

    // ==========================================================================
    // 发布
    // ==========================================================================

    /// 发布一条消息到指定 topic
    /// @param topic   订阅主题（ZMQ 过滤前缀），如 "notice"、"market.BTC-USDT"
    /// @param msgType 消息类型
    /// @param body    消息体（protobuf 序列化字节）
    /// @return true-发送成功
    bool Publish(const std::string& topic, uint16_t msgType,
                 const std::vector<uint8_t>& body);

    /// 为下一条 Publish 设置 message_id（用于客户端去重）
    void SetMessageId(uint64_t id) { nextMsgId_ = id; }

    /// 获取并自增 message_id
    uint64_t NextMessageId() { return msgSeq_.fetch_add(1) + 1; }

private:
    void* pubSock_ = nullptr;
    std::atomic<uint64_t> msgSeq_{0};
    uint64_t nextMsgId_ = 0;
};

} // namespace framework

#endif // FRAMEWORK_PUB_SERVER_H
