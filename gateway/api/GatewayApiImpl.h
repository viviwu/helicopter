/**
  ******************************************************************************
  * @file           : GatewayApiImpl.h
  * @author         : vivi wu
  * @brief          : GatewayApi实现（隐藏所有网络、线程、protobuf细节）
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#ifndef GATEWAY_API_IMPL_H
#define GATEWAY_API_IMPL_H

#include "api/GatewayApi.h"
#include <atomic>
#include <string>
#include <thread>

namespace gateway {

class GatewayApiImpl : public GatewayApi {
public:
    GatewayApiImpl();
    ~GatewayApiImpl() override;

    void Release() override;
    void RegisterSpi(GatewaySpi* pSpi) override;
    int Init(const char* frontAddress, int port, int heartbeatIntervalSec) override;
    int Login(const LoginRequest& req) override;

private:
    void RecvThreadFunc();
    void Disconnect();
    void JoinThread();

    std::string frontAddress_;
    int port_ = 12345;
    int heartbeatIntervalSec_ = 5;
    GatewaySpi* spi_ = nullptr;
    int sockfd_ = -1;
    std::atomic<bool> running_{false};
    std::thread recvThread_;
};

} // namespace gateway

#endif // GATEWAY_API_IMPL_H
