/**
  ******************************************************************************
  * @file           : GatewayApiImpl.h
  * @author         : vivi wu
  * @brief          : GatewayApi 实现（ZMQ_DEALER，隐藏网络/线程/protobuf 细节）
  * @version        : 0.2.0
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
    void* dealerSock_ = nullptr;
    std::atomic<bool> running_{false};
    std::thread recvThread_;
};

} // namespace gateway

#endif // GATEWAY_API_IMPL_H
