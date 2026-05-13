/**
 **************************************************************************
 * @file    :GatewayError.h
 * @author  :viviwu
 * @brief   :Gateway错误码定义与工具
 * @version :0.1
 * @date    :5/12/26 PM3:31
 * **************************************************************************
 */

#ifndef GATEWAYERROR_H
#define GATEWAYERROR_H

#include <cstring>

namespace gateway {

enum class GatewayError : int {
  kSuccess = 0,

  // 网络层错误 (1001-1099)
  kNetworkError = 1001,
  kTimeout = 1002,
  kConnectionClosed = 1003,
  kSendFailed = 1004,
  kRecvFailed = 1005,

  // 会话层错误 (2001-2099)
  kLoginFailed = 2001,
  kNotLogin = 2002,
  kSessionNotFound = 2003,
  kAlreadyLogin = 2004,

  // 业务层错误 (3001-3099)
  kOrderRejected = 3001,
  kOrderNotFound = 3002,
  kInvalidSymbol = 3003,
  kInvalidPrice = 3004,
  kInvalidVolume = 3005,

  // 协议层错误 (4001-4099)
  kInvalidPacket = 4001,
  kPacketTooLarge = 4002,
  kUnknownCmd = 4003,

  // 系统错误 (9001-9999)
  kInternalError = 9001,
  kNotInitialized = 9002,
  kConfigError = 9003,
};

inline const char* GetErrorString(GatewayError err) {
  switch (err) {
    case GatewayError::kSuccess:          return "Success";
    case GatewayError::kNetworkError:     return "Network error";
    case GatewayError::kTimeout:          return "Timeout";
    case GatewayError::kConnectionClosed: return "Connection closed";
    case GatewayError::kSendFailed:       return "Send failed";
    case GatewayError::kRecvFailed:       return "Receive failed";
    case GatewayError::kLoginFailed:      return "Login failed";
    case GatewayError::kNotLogin:         return "Not logged in";
    case GatewayError::kSessionNotFound:  return "Session not found";
    case GatewayError::kAlreadyLogin:     return "Already logged in";
    case GatewayError::kOrderRejected:    return "Order rejected";
    case GatewayError::kOrderNotFound:    return "Order not found";
    case GatewayError::kInvalidSymbol:    return "Invalid symbol";
    case GatewayError::kInvalidPrice:     return "Invalid price";
    case GatewayError::kInvalidVolume:    return "Invalid volume";
    case GatewayError::kInvalidPacket:    return "Invalid packet";
    case GatewayError::kPacketTooLarge:   return "Packet too large";
    case GatewayError::kUnknownCmd:       return "Unknown command";
    case GatewayError::kInternalError:    return "Internal error";
    case GatewayError::kNotInitialized:   return "Not initialized";
    case GatewayError::kConfigError:      return "Configuration error";
    default:                              return "Unknown error";
  }
}

}  // namespace gateway

#endif  // GATEWAYERROR_H
