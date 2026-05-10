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
    kLoginRequest           = 1,
    kLoginResponse          = 2,
    kPing                   = 3,  // 客户端 → 服务端：应用层心跳请求
    kPong                   = 4,  // 服务端 → 客户端：应用层心跳响应
    kBroadcastNotification  = 5,  // 服务端 → 客户端（PUB）：广播通知
    // 后续拓展:
    // kQueryOrderRequest  = 6,
    // kQueryOrderResponse = 7,
    // kSendOrderRequest   = 8,
};

} // namespace gateway

#endif // GATEWAY_MSG_TYPES_H
