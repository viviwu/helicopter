/**
 ******************************************************************************
 * @file           : QuoteApi.cpp
 * @author         : vivi wu
 * @brief          : 行情网关客户端 SDK 实现（纯 SUB 模式 / 本地订阅）
 * @version        : 0.3.0
 * @date           : 13/05/26
 ******************************************************************************
 */
#include "../include/gateway/api/quote_api.h"

#include "quote.pb.h"

#include <iostream>

namespace quote {

// ============================================================================
// 构造 / 析构
// ============================================================================

quote_api::quote_api() {
    zmq_impl_.on_connected = [this]() {
        if (spi_) spi_->OnFrontConnected();
    };
    zmq_impl_.on_disconnected = [this](int reason) {
        if (spi_) spi_->OnFrontDisconnected(reason);
    };
    zmq_impl_.on_pub_message = [this](const std::string& topic, uint16_t msgType,
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
    };
}

quote_api::~quote_api() = default;

// ============================================================================
// 连接管理
// ============================================================================

int quote_api::Connect(const char* address, int pubPort) {
    return zmq_impl_.Connect(address, pubPort);
}

void quote_api::Disconnect() {
    zmq_impl_.Disconnect();
}

} // namespace quote
