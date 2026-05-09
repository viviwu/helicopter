/**
  ******************************************************************************
  * @file           : AuthHandler.h
  * @author         : vivi wu
  * @brief          : 登录认证业务处理器（实现 IGatewayHandler）
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#ifndef GATEWAY_SERVER_AUTH_HANDLER_H
#define GATEWAY_SERVER_AUTH_HANDLER_H

#include "server/Gateway.h"

#include <string>

namespace gateway_proto {
class AccountInfo;
}

namespace gateway {

class AuthHandler : public IGatewayHandler {
public:
    AuthHandler() = default;
    ~AuthHandler() override = default;

    bool OnLogin(const gateway_proto::LoginRequest& req,
                 gateway_proto::LoginResponse& rsp) override;

private:
    bool authenticate(const std::string& user, const std::string& pass,
                      gateway_proto::AccountInfo& outAccount);
};

} // namespace gateway

#endif // GATEWAY_SERVER_AUTH_HANDLER_H
