/**
  ******************************************************************************
  * @file           : QuoteGatewayApi.cpp
  * @author         : vivi wu
  * @brief          : 行情网关客户端 SDK 实现（纯 SUB 模式 / 本地订阅）
  * @version        : 0.2.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#include "quote/QuoteGatewayApi.h"

#include "quote.pb.h"

#include <iostream>

namespace quote {

// ============================================================================
// 连接
// ============================================================================

int QuoteGatewayApi::Connect(const char* address, int pubPort) {
    // routerPort=0 表示纯 SUB 模式（无 DEALER），heartbeatSec 忽略
    return GatewayApi::Connect(address, 0, pubPort, 0);
}

// ============================================================================
// 回调
// ============================================================================

void QuoteGatewayApi::OnConnected() {
    if (spi_) spi_->OnFrontConnected();
}

void QuoteGatewayApi::OnDisconnected(int reason) {
    if (spi_) spi_->OnFrontDisconnected(reason);
}

void QuoteGatewayApi::OnRouterMessage(uint16_t /*msgType*/,
                                const std::vector<uint8_t>& /*body*/) {
    // 纯 SUB 模式，不会有 Router 消息
}

void QuoteGatewayApi::OnPubMessage(const std::string& topic, uint16_t msgType,
                             const std::vector<uint8_t>& body) {
    if (!spi_) return;

    auto type = static_cast<QuoteMsgType>(msgType);

    switch (type) {
    case QuoteMsgType::kMarketData: {
        quote_proto::MarketData proto;
        if (proto.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            MarketData md;
            md.symbol    = proto.symbol();
            md.price     = proto.price();
            md.volume    = proto.volume();
            md.timestamp = proto.timestamp();
            spi_->OnMarketData(md);
        }
        break;
    }
    case QuoteMsgType::kNotice: {
        quote_proto::Notice proto;
        if (proto.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            // 去重
            uint64_t msgId = proto.message_id();
            if (msgId != 0 && msgId == lastNoticeId_) {
                return;  // duplicate
            }
            lastNoticeId_ = msgId;

            Notice notice;
            notice.content    = proto.content();
            notice.timestamp  = proto.timestamp();
            notice.message_id = msgId;
            spi_->OnNotice(notice);
        }
        break;
    }
    default:
        break;
    }
}

} // namespace quote
