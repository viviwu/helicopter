/**
  ******************************************************************************
  * @file           : AuthHandler.cpp
  * @author         : vivi wu
  * @brief          : 登录认证业务实现
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#include "server/AuthHandler.h"
#include "gateway.pb.h"

namespace gateway {

bool AuthHandler::authenticate(const std::string& user, const std::string& pass,
                               gateway_proto::AccountInfo& outAccount) {
    if (user == "admin" && pass == "123456") {
        outAccount.set_user_id(1001);
        outAccount.set_username("admin");
        outAccount.set_email("admin@example.com");
        outAccount.set_role("admin");
        return true;
    }
    return false;
}

bool AuthHandler::OnLogin(const gateway_proto::LoginRequest& req,
                          gateway_proto::LoginResponse& rsp) {
    rsp.set_request_id(req.request_id());

    gateway_proto::AccountInfo* account = rsp.mutable_account();
    if (authenticate(req.username(), req.password(), *account)) {
        rsp.set_error_code(0);
        rsp.set_error_msg("Login successful");
    } else {
        rsp.set_error_code(1);
        rsp.set_error_msg("Invalid username or password");
    }
    return true;
}

} // namespace gateway
