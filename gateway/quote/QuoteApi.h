/**
  ******************************************************************************
  * @file           : QuoteGatewayApi.h
  * @author         : vivi wu
  * @brief          : 行情网关客户端 SDK（纯 SUB 模式 / 本地主题订阅）
  * @version        : 0.2.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef QUOTE_QUOTE_API_H
#define QUOTE_QUOTE_API_H

#include "framework/GatewayApi.h"
#include "quote/QuoteTypes.h"

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
class QuoteApi : public framework::GatewayApi {
public:
    void RegisterSpi(QuoteSpi* spi) { spi_ = spi; }

    /// 连接到 QuoteGatewayServer（纯 SUB 模式）
    /// @param address  QuoteGatewayServer 地址
    /// @param pubPort  PUB 端口
    /// @return 0-成功
    int Connect(const char* address, int pubPort);

    using GatewayApi::Subscribe;    // 继承基类方法
    using GatewayApi::Unsubscribe;  // 继承基类方法

protected:
    void OnConnected() override;
    void OnDisconnected(int reason) override;
    void OnRouterMessage(uint16_t msgType, const std::vector<uint8_t>& body) override;
    void OnPubMessage(const std::string& topic, uint16_t msgType,
                      const std::vector<uint8_t>& body) override;

private:
    QuoteSpi* spi_ = nullptr;
    uint64_t lastNoticeId_ = 0;   // 通知去重
};

} // namespace quote

#endif // QUOTE_QUOTE_API_H
