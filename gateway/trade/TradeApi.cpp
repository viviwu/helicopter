/**
  ******************************************************************************
  * @file           : TradeApi.cpp
  * @author         : vivi wu
  * @brief          : 交易网关客户端 SDK 实现
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#include "trade/TradeApi.h"

// protobuf 仅在实现文件中可见
#include "trade.pb.h"

#include <iostream>

namespace trade {

// ============================================================================
// 辅助：业务 struct ↔ protobuf 转换
// ============================================================================

static trade_proto::LoginRequest ToProto(const LoginRequest& req) {
    trade_proto::LoginRequest p;
    p.set_request_id(req.request_id);
    p.set_username(req.username);
    p.set_password(req.password);
    return p;
}

static LoginResponse FromProto(const trade_proto::LoginResponse& p) {
    LoginResponse r;
    r.request_id = p.request_id();
    r.error_code = p.error_code();
    r.error_msg  = p.error_msg();
    if (p.has_account()) {
        r.account.user_id  = p.account().user_id();
        r.account.username = p.account().username();
        r.account.email    = p.account().email();
        r.account.role     = p.account().role();
    }
    return r;
}

static trade_proto::PlaceOrderRequest ToProto(const PlaceOrderRequest& req) {
    trade_proto::PlaceOrderRequest p;
    p.set_request_id(req.request_id);
    p.set_symbol(req.symbol);
    p.set_side(req.side);
    p.set_order_type(req.order_type);
    p.set_price(req.price);
    p.set_quantity(req.quantity);
    return p;
}

static PlaceOrderResponse FromProto(const trade_proto::PlaceOrderResponse& p) {
    PlaceOrderResponse r;
    r.request_id = p.request_id();
    r.error_code = p.error_code();
    r.error_msg  = p.error_msg();
    r.order_id   = p.order_id();
    return r;
}

static trade_proto::QueryOrderRequest ToProto(const QueryOrderRequest& req) {
    trade_proto::QueryOrderRequest p;
    p.set_request_id(req.request_id);
    p.set_order_id(req.order_id);
    return p;
}

static QueryOrderResponse FromProto(const trade_proto::QueryOrderResponse& p) {
    QueryOrderResponse r;
    r.request_id = p.request_id();
    r.error_code = p.error_code();
    r.error_msg  = p.error_msg();
    r.order_id   = p.order_id();
    r.symbol     = p.symbol();
    r.status     = p.status();
    r.filled_qty = p.filled_qty();
    r.avg_price  = p.avg_price();
    return r;
}

static trade_proto::CancelOrderRequest ToProto(const CancelOrderRequest& req) {
    trade_proto::CancelOrderRequest p;
    p.set_request_id(req.request_id);
    p.set_order_id(req.order_id);
    return p;
}

static CancelOrderResponse FromProto(const trade_proto::CancelOrderResponse& p) {
    CancelOrderResponse r;
    r.request_id = p.request_id();
    r.error_code = p.error_code();
    r.error_msg  = p.error_msg();
    return r;
}

// ============================================================================
// API 方法
// ============================================================================

int TradeApi::Login(const LoginRequest& req) {
    auto proto = ToProto(req);
    std::string data = proto.SerializeAsString();
    std::vector<uint8_t> body(data.begin(), data.end());
    if (!SendToRouter(static_cast<uint16_t>(TradeMsgType::kLoginRequest), body)) {
        return ERR_SEND_FAILED;
    }
    return ERR_OK;
}

int TradeApi::PlaceOrder(const PlaceOrderRequest& req) {
    auto proto = ToProto(req);
    std::string data = proto.SerializeAsString();
    std::vector<uint8_t> body(data.begin(), data.end());
    if (!SendToRouter(static_cast<uint16_t>(TradeMsgType::kPlaceOrderRequest), body)) {
        return ERR_SEND_FAILED;
    }
    return ERR_OK;
}

int TradeApi::QueryOrder(const QueryOrderRequest& req) {
    auto proto = ToProto(req);
    std::string data = proto.SerializeAsString();
    std::vector<uint8_t> body(data.begin(), data.end());
    if (!SendToRouter(static_cast<uint16_t>(TradeMsgType::kQueryOrderRequest), body)) {
        return ERR_SEND_FAILED;
    }
    return ERR_OK;
}

int TradeApi::CancelOrder(const CancelOrderRequest& req) {
    auto proto = ToProto(req);
    std::string data = proto.SerializeAsString();
    std::vector<uint8_t> body(data.begin(), data.end());
    if (!SendToRouter(static_cast<uint16_t>(TradeMsgType::kCancelOrderRequest), body)) {
        return ERR_SEND_FAILED;
    }
    return ERR_OK;
}

// ============================================================================
// 回调
// ============================================================================

void TradeApi::OnConnected() {
    if (spi_) spi_->OnFrontConnected();
}

void TradeApi::OnDisconnected(int reason) {
    if (spi_) spi_->OnFrontDisconnected(reason);
}

void TradeApi::OnRouterMessage(uint16_t msgType, const std::vector<uint8_t>& body) {
    if (!spi_) return;

    auto type = static_cast<TradeMsgType>(msgType);

    switch (type) {
    case TradeMsgType::kLoginResponse: {
        trade_proto::LoginResponse proto;
        if (proto.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            spi_->OnLogin(FromProto(proto));
        }
        break;
    }
    case TradeMsgType::kPlaceOrderResponse: {
        trade_proto::PlaceOrderResponse proto;
        if (proto.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            spi_->OnPlaceOrder(FromProto(proto));
        }
        break;
    }
    case TradeMsgType::kQueryOrderResponse: {
        trade_proto::QueryOrderResponse proto;
        if (proto.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            spi_->OnQueryOrder(FromProto(proto));
        }
        break;
    }
    case TradeMsgType::kCancelOrderResponse: {
        trade_proto::CancelOrderResponse proto;
        if (proto.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            spi_->OnCancelOrder(FromProto(proto));
        }
        break;
    }
    default:
        break;
    }
}

void TradeApi::OnPubMessage(const std::string& /*topic*/, uint16_t /*msgType*/,
                             const std::vector<uint8_t>& /*body*/) {
    // TradeApi 不订阅 PUB 消息
}

} // namespace trade
