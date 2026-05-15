/**
 ******************************************************************************
 * @file           : TradeApi.h
 * @author         : vivi wu
 * @brief          : 交易网关客户端 SDK（DEALER 模式）
 * @version        : 0.2.0
 * @date           : 13/05/26
 ******************************************************************************
 */
#ifndef TRADE_TRADE_API_H
#define TRADE_TRADE_API_H

#include "../../../src/zmq_impl/trade_api_zmq_impl.h"
#include "TradeTypes.h"

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
class TradeApi {
public:
    TradeApi();
    ~TradeApi();

    TradeApi(const TradeApi&) = delete;
    TradeApi& operator=(const TradeApi&) = delete;

    void RegisterSpi(TradeSpi* spi) { spi_ = spi; }

    int Connect(const char* address, int routerPort, int heartbeatSec);
    void Disconnect();
    bool IsConnected() const { return zmq_impl_.IsConnected(); }

    // 交易 API
    int Login(const LoginRequest& req);
    int PlaceOrder(const PlaceOrderRequest& req);
    int QueryOrder(const QueryOrderRequest& req);
    int CancelOrder(const CancelOrderRequest& req);

private:
    TradeSpi* spi_ = nullptr;
    TradeApiZmqImpl zmq_impl_;
    uint64_t lastBroadcastId_ = 0;
};

} // namespace trade

#endif // TRADE_TRADE_API_H
