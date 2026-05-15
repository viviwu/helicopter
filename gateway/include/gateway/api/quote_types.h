/**
  ******************************************************************************
  * @file           : QuoteTypes.h
  * @author         : vivi wu
  * @brief          : 行情网关业务层数据类型（不依赖 protobuf）
  * @version        : 0.2.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef GATEWAY_QUOTE_TYPES_H
#define GATEWAY_QUOTE_TYPES_H

#include <cstdint>
#include <string>

namespace quote {

// ============================================================================
// 消息类型枚举（client/server 共享）
// ============================================================================
enum class QuoteMsgType : uint16_t {
    kMarketData = 1,
    kNotice     = 2,
};

// ============================================================================
// 行情数据
// ============================================================================
struct MarketData {
    std::string symbol;
    double price = 0.0;
    double volume = 0.0;
    int64_t timestamp = 0;
};

// ============================================================================
// 通知
// ============================================================================
struct Notice {
    std::string content;
    int64_t timestamp = 0;
    uint64_t message_id = 0;
};

} // namespace quote

#endif // GATEWAY_QUOTE_TYPES_H
