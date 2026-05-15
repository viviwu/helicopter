/**
  ******************************************************************************
  * @file           : TradeTypes.h
  * @author         : vivi wu
  * @brief          : 交易网关业务层数据类型（不依赖 protobuf）
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef TRADE_TRADE_TYPES_H
#define TRADE_TRADE_TYPES_H

#include <cstdint>
#include <string>

namespace trade {

// ============================================================================
// 消息类型枚举（client/server 共享）
// ============================================================================
enum class TradeMsgType : uint16_t {
    kLoginRequest       = 1,
    kLoginResponse      = 2,
    kPing               = 3,
    kPong               = 4,
    kPlaceOrderRequest  = 5,
    kPlaceOrderResponse = 6,
    kQueryOrderRequest  = 7,
    kQueryOrderResponse = 8,
    kCancelOrderRequest  = 9,
    kCancelOrderResponse = 10,
};

// ============================================================================
// 错误码
// ============================================================================
constexpr int ERR_OK                   = 0;
constexpr int ERR_NETWORK              = -1;
constexpr int ERR_SEND_FAILED          = -2;
constexpr int ERR_INVALID_CREDENTIALS  = 1;
constexpr int ERR_USER_LOCKED          = 2;

// ============================================================================
// 账户
// ============================================================================
struct AccountInfo {
    uint64_t user_id = 0;
    std::string username;
    std::string email;
    std::string role;
};

// ============================================================================
// 登录
// ============================================================================
struct LoginRequest {
    uint64_t request_id = 0;
    std::string username;
    std::string password;
};

struct LoginResponse {
    uint64_t request_id = 0;
    int error_code = ERR_OK;
    std::string error_msg;
    AccountInfo account;
};

// ============================================================================
// 下单
// ============================================================================
struct PlaceOrderRequest {
    uint64_t request_id = 0;
    std::string symbol;
    int side = 0;       // 1=buy, 2=sell
    int order_type = 0; // 1=limit, 2=market
    double price = 0.0;
    double quantity = 0.0;
};

struct PlaceOrderResponse {
    uint64_t request_id = 0;
    int error_code = ERR_OK;
    std::string error_msg;
    uint64_t order_id = 0;
};

// ============================================================================
// 查询订单
// ============================================================================
struct QueryOrderRequest {
    uint64_t request_id = 0;
    uint64_t order_id = 0;
};

struct QueryOrderResponse {
    uint64_t request_id = 0;
    int error_code = ERR_OK;
    std::string error_msg;
    uint64_t order_id = 0;
    std::string symbol;
    int status = 0;         // 0=pending, 1=filled, 2=cancelled
    double filled_qty = 0.0;
    double avg_price = 0.0;
};

// ============================================================================
// 撤单
// ============================================================================
struct CancelOrderRequest {
    uint64_t request_id = 0;
    uint64_t order_id = 0;
};

struct CancelOrderResponse {
    uint64_t request_id = 0;
    int error_code = ERR_OK;
    std::string error_msg;
};

} // namespace trade

#endif // TRADE_TRADE_TYPES_H
