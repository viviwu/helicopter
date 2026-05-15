/**
  ******************************************************************************
  * @file           : QuoteGateway.cpp
  * @author         : vivi wu
  * @brief          : 行情网关服务端实现（纯 PUB 模式）
  * @version        : 0.2.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#include "quote_gateway.h"
#include "quote.pb.h"

#include <chrono>
#include <iostream>

namespace quote {

QuoteGateway::~QuoteGateway() {
    Stop();
}

bool QuoteGateway::Start(const char* bindAddr, int port) {
    return pubServer_.Start(bindAddr, port);
}

void QuoteGateway::Stop() {
    pubServer_.Stop();
}

bool QuoteGateway::PublishMarketData(const std::string& symbol, double price,
                                     double volume) {
    quote_proto::MarketData md;
    md.set_symbol(symbol);
    md.set_price(price);
    md.set_volume(volume);
    md.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
    );

    std::string data = md.SerializeAsString();
    std::vector<uint8_t> body(data.begin(), data.end());

    std::string topic = std::string(QuoteTopic::kMarketPrefix) + symbol;
    bool ok = pubServer_.Publish(topic,
        static_cast<uint16_t>(QuoteMsgType::kMarketData), body);

    if (ok) {
        std::cout << "[QuoteGateway] MarketData → " << topic
                  << ": " << symbol << " = " << price << "\n";
    }
    return ok;
}

bool QuoteGateway::PublishNotice(const std::string& content) {
    uint64_t msgId = pubServer_.NextMessageId();

    quote_proto::Notice notice;
    notice.set_content(content);
    notice.set_message_id(msgId);
    notice.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
    );

    std::string data = notice.SerializeAsString();
    std::vector<uint8_t> body(data.begin(), data.end());

    bool ok = pubServer_.Publish(QuoteTopic::kNotice,
        static_cast<uint16_t>(QuoteMsgType::kNotice), body);

    if (ok) {
        std::cout << "[QuoteGateway] Notice published (msg_id=" << msgId
                  << "): " << content << "\n";
    }
    return ok;
}

} // namespace quote
