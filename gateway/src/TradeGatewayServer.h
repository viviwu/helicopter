/**
  ******************************************************************************
  * @file           : TradeGatewayServer.h
  * @author         : vivi wu
  * @brief          : 交易网关服务端（ROUTER 模式 / 请求-应答）
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef TRADE_TRADE_SERVER_H
#define TRADE_TRADE_SERVER_H

#include "../include/gateway/api/TradeTypes.h"
#include "RouterServer.h"
#include "trade/TradeHandler.h"

namespace trade {

/// 交易网关服务端
/// 继承 RouterServer，将消息分派到 TradeHandler
class TradeGatewayServer : public framework::RouterServer {
public:
    void RegisterHandler(TradeHandler* handler) { handler_ = handler; }

protected:
    void OnMessage(const std::vector<uint8_t>& identity,
                   uint16_t msgType,
                   const std::vector<uint8_t>& body) override;

private:
    TradeHandler* handler_ = nullptr;
};

} // namespace trade

#endif // TRADE_TRADE_SERVER_H
