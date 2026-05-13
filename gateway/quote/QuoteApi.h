/**
 ******************************************************************************
 * @file           : QuoteApi.h
 * @author         : vivi wu
 * @brief          : 行情网关客户端 SDK（纯 SUB 模式 / 本地主题订阅）
 * @version        : 0.3.0
 * @date           : 13/05/26
 ******************************************************************************
 */
#ifndef QUOTE_QUOTE_API_H
#define QUOTE_QUOTE_API_H

#include "quote/QuoteTypes.h"
#include "quote/quote_api_zmq_impl.h"

namespace quote {

/// 行情网关回调接口
class QuoteSpi {
public:
    virtual ~QuoteSpi() = default;

    virtual void OnFrontConnected() {}
    virtual void OnFrontDisconnected(int reason) {}

    /// 收到行情数据
    virtual void OnMarketData(const MarketData& data) {}

    /// 收到通知
    virtual void OnNotice(const Notice& notice) {}
};

/// 行情网关客户端 SDK
/// 通过 SUB socket 连接 QuoteGatewayServer 的 PUB 端口，
/// 使用本地 Subscribe/Unsubscribe 管理主题过滤。
class QuoteApi {
public:
    QuoteApi();
    ~QuoteApi();

    QuoteApi(const QuoteApi&) = delete;
    QuoteApi& operator=(const QuoteApi&) = delete;

    void RegisterSpi(QuoteSpi* spi) { spi_ = spi; }

    int Connect(const char* address, int pubPort);
    void Disconnect();
    bool IsConnected() const { return zmq_impl_.IsConnected(); }

    bool Subscribe(const std::string& topic) { return zmq_impl_.Subscribe(topic); }
    bool Unsubscribe(const std::string& topic) { return zmq_impl_.Unsubscribe(topic); }

private:
    QuoteSpi* spi_ = nullptr;
    QuoteApiZmqImpl zmq_impl_;
    uint64_t lastNoticeId_ = 0;
};

} // namespace quote

#endif // QUOTE_QUOTE_API_H
