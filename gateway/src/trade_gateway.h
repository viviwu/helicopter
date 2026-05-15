/**
  ******************************************************************************
  * @file           : TradeGateway.h
  * @author         : vivi wu
  * @brief          : 交易网关服务端（ROUTER 模式 / 请求-应答）
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef GATEWAY_TRADE_SERVER_H
#define GATEWAY_TRADE_SERVER_H

#include "RouterServer.h"
#include "TradeHandler.h"
#include "gateway/api/trade_types.h"

namespace trade {

/// 交易网关服务端
/// 继承 RouterServer，将消息分派到 TradeHandler
class TradeGateway : public framework::RouterServer {
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

#endif // GATEWAY_TRADE_SERVER_H
