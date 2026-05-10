/**
  ******************************************************************************
  * @file           : TradeGatewayServer.cpp
  * @author         : vivi wu
  * @brief          : 交易网关服务端实现
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#include "trade/TradeGatewayServer.h"
#include "trade.pb.h"

#include <iostream>

namespace trade {

void TradeGatewayServer::OnMessage(const std::vector<uint8_t>& identity,
                             uint16_t msgType,
                             const std::vector<uint8_t>& body) {
    if (!IsRunning()) return;

    auto type = static_cast<TradeMsgType>(msgType);

    auto sendProtoReply = [&](uint16_t replyType, const std::string& data) {
        std::vector<uint8_t> respBody(data.begin(), data.end());
        SendReply(identity, replyType, respBody);
    };

    switch (type) {
    case TradeMsgType::kLoginRequest: {
        trade_proto::LoginRequest req;
        if (!req.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            std::cerr << "[TradeGatewayServer] Failed to parse LoginRequest\n";
            return;
        }

        trade_proto::LoginResponse rsp;
        if (handler_ && handler_->OnLogin(req, rsp)) {
            sendProtoReply(static_cast<uint16_t>(TradeMsgType::kLoginResponse),
                           rsp.SerializeAsString());
        }
        break;
    }
    case TradeMsgType::kPlaceOrderRequest: {
        trade_proto::PlaceOrderRequest req;
        if (!req.ParseFromArray(body.data(), static_cast<int>(body.size()))) return;

        trade_proto::PlaceOrderResponse rsp;
        if (handler_ && handler_->OnPlaceOrder(req, rsp)) {
            sendProtoReply(static_cast<uint16_t>(TradeMsgType::kPlaceOrderResponse),
                           rsp.SerializeAsString());
        }
        break;
    }
    case TradeMsgType::kQueryOrderRequest: {
        trade_proto::QueryOrderRequest req;
        if (!req.ParseFromArray(body.data(), static_cast<int>(body.size()))) return;

        trade_proto::QueryOrderResponse rsp;
        if (handler_ && handler_->OnQueryOrder(req, rsp)) {
            sendProtoReply(static_cast<uint16_t>(TradeMsgType::kQueryOrderResponse),
                           rsp.SerializeAsString());
        }
        break;
    }
    case TradeMsgType::kCancelOrderRequest: {
        trade_proto::CancelOrderRequest req;
        if (!req.ParseFromArray(body.data(), static_cast<int>(body.size()))) return;

        trade_proto::CancelOrderResponse rsp;
        if (handler_ && handler_->OnCancelOrder(req, rsp)) {
            sendProtoReply(static_cast<uint16_t>(TradeMsgType::kCancelOrderResponse),
                           rsp.SerializeAsString());
        }
        break;
    }
    default:
        std::cerr << "[TradeGatewayServer] Unknown message type: " << msgType << "\n";
        break;
    }
}

} // namespace trade
