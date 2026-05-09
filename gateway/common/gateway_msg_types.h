/**
  ******************************************************************************
  * @file           : gateway_msg_types.h
  * @author         : vivi wu
  * @brief          : 消息类型枚举（TCP 协议层，client/server 共享）
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#ifndef GATEWAY_MSG_TYPES_H
#define GATEWAY_MSG_TYPES_H

#include <cstdint>

namespace gateway {

enum class GatewayMsgType : uint16_t {
    kLoginRequest  = 1,
    kLoginResponse = 2,
    // 后续拓展:
    // kQueryOrderRequest  = 3,
    // kQueryOrderResponse = 4,
    // kSendOrderRequest   = 5,
};

} // namespace gateway

#endif // GATEWAY_MSG_TYPES_H
