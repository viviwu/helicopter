/**
  ******************************************************************************
  * @file           : TradeHandler.h
  * @author         : vivi wu
  * @brief          : 交易网关业务处理器接口
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef TRADE_TRADE_HANDLER_H
#define TRADE_TRADE_HANDLER_H

namespace trade_proto {
class LoginRequest;
class LoginResponse;
class PlaceOrderRequest;
class PlaceOrderResponse;
class QueryOrderRequest;
class QueryOrderResponse;
class CancelOrderRequest;
class CancelOrderResponse;
}

namespace trade {

/// 交易网关业务处理器接口
/// 每个方法对应一种交易消息类型，由 TradeGateway 在收到消息后调用
class TradeHandler {
public:
    virtual ~TradeHandler() = default;

    virtual bool OnLogin(const trade_proto::LoginRequest& req,
                         trade_proto::LoginResponse& rsp) = 0;

    virtual bool OnPlaceOrder(const trade_proto::PlaceOrderRequest& req,
                              trade_proto::PlaceOrderResponse& rsp) { return false; }

    virtual bool OnQueryOrder(const trade_proto::QueryOrderRequest& req,
                              trade_proto::QueryOrderResponse& rsp) { return false; }

    virtual bool OnCancelOrder(const trade_proto::CancelOrderRequest& req,
                               trade_proto::CancelOrderResponse& rsp) { return false; }
};

} // namespace trade

#endif // TRADE_TRADE_HANDLER_H
