/**
  ******************************************************************************
  * @file           : GatewayApiDef.h
  * @author         : vivi wu
  * @brief          : Gateway 业务层数据结构与错误码定义（不依赖 protobuf）
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#ifndef GATEWAY_API_DEF_H
#define GATEWAY_API_DEF_H

#include <cstdint>
#include <string>

namespace gateway {

// ============================================================================
// 错误码常量
// ============================================================================
constexpr int ERR_OK = 0;                   // 成功
constexpr int ERR_NETWORK = -1;             // 网络/连接错误
constexpr int ERR_SEND_FAILED = -2;         // 发送失败
constexpr int ERR_INVALID_CREDENTIALS = 1;  // 用户名或密码错误
constexpr int ERR_USER_LOCKED = 2;          // 用户被锁定

// ============================================================================
// 业务层数据结构（与 protobuf 解耦）
// ============================================================================

struct AccountInfo {
    uint64_t user_id = 0;
    std::string username;
    std::string email;
    std::string role;
};

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

} // namespace gateway

#endif // GATEWAY_API_DEF_H
