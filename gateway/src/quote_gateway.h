/**
  ******************************************************************************
  * @file           : QuoteGateway.h
  * @author         : vivi wu
  * @brief          : 行情网关服务端（PUB 模式 / 主题数据发布）
  * @version        : 0.2.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef QUOTE_QUOTE_SERVER_H
#define QUOTE_QUOTE_SERVER_H

#include "../include/gateway/api/quote_types.h"
#include "PubServer.h"

#include <string>

namespace quote {

/// Topic 常量
struct QuoteTopic {
    static constexpr const char* kNotice = "notice";
    static constexpr const char* kMarketPrefix = "market.";
};

/// 行情网关服务端
/// 基于 PubServer，发布行情数据和通知到指定 topic。
/// 客户端通过 QuoteApi 的 SUB socket 本地订阅/取消订阅。
class QuoteGateway {
public:
    QuoteGateway() = default;
    ~QuoteGateway();

    // ==========================================================================
    // 生命周期
    // ==========================================================================

    bool Start(const char* bindAddr, int port);
    void Stop();

    // ==========================================================================
    // 发布
    // ==========================================================================

    /// 发布行情数据到 "market.{symbol}" topic
    bool PublishMarketData(const std::string& symbol, double price, double volume);

    /// 发布通知到 "notice" topic
    bool PublishNotice(const std::string& content);

private:
    framework::PubServer pubServer_;
};

} // namespace quote

#endif // QUOTE_QUOTE_SERVER_H
