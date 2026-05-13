/**
  ******************************************************************************
  * @file           : TradeGatewayApi.h
  * @author         : vivi wu
  * @brief          : 交易网关客户端 SDK（DEALER 模式）
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef TRADE_TRADE_API_H
#define TRADE_TRADE_API_H

#include "framework/GatewayApi.h"
#include "trade/TradeTypes.h"

namespace trade {

/// 交易网关回调接口
class TradeSpi {
public:
    virtual ~TradeSpi() = default;

    virtual void OnFrontConnected() {}
    virtual void OnFrontDisconnected(int reason) {}
    virtual void OnLogin(const LoginResponse& rsp) = 0;
    virtual void OnPlaceOrder(const PlaceOrderResponse& rsp) {}
    virtual void OnQueryOrder(const QueryOrderResponse& rsp) {}
    virtual void OnCancelOrder(const CancelOrderResponse& rsp) {}
};

/// 交易网关客户端 SDK
class TradeApi : public framework::GatewayApi {
public:
    void RegisterSpi(TradeSpi* spi) { spi_ = spi; }

    // 交易 API
    int Login(const LoginRequest& req);
    int PlaceOrder(const PlaceOrderRequest& req);
    int QueryOrder(const QueryOrderRequest& req);
    int CancelOrder(const CancelOrderRequest& req);

protected:
    void OnConnected() override;
    void OnDisconnected(int reason) override;
    void OnRouterMessage(uint16_t msgType, const std::vector<uint8_t>& body) override;
    void OnPubMessage(const std::string& topic, uint16_t msgType,
                      const std::vector<uint8_t>& body) override;

private:
    TradeSpi* spi_ = nullptr;
    uint64_t lastBroadcastId_ = 0;  // dedup for notice messages if received
};

} // namespace trade

#endif // TRADE_TRADE_API_H
