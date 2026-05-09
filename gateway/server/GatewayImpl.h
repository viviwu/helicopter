/**
  ******************************************************************************
  * @file           : GatewayImpl.h
  * @author         : vivi wu
  * @brief          : Gateway 服务端实现（TCP server / 消息收发 / protobuf 解析归档）
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#ifndef GATEWAY_SERVER_GATEWAY_IMPL_H
#define GATEWAY_SERVER_GATEWAY_IMPL_H

#include "server/Gateway.h"
#include "common/ThreadPool.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace gateway {

class GatewayImpl : public Gateway {
public:
    GatewayImpl();
    ~GatewayImpl() override;

    void Release() override;
    void RegisterHandler(IGatewayHandler* handler) override;
    void SetThreadPool(ThreadPool* pool) override;
    void SetMaxConnections(int max) override;
    void SetIdleTimeout(int seconds) override;
    bool Start(const char* bindAddr, int port) override;
    void Stop() override;

private:
    void AcceptThreadFunc();
    void ClientThreadFunc(int clientSock);
    void CloseAllClientSockets();

    IGatewayHandler* handler_ = nullptr;
    ThreadPool* threadPool_ = nullptr;
    int serverSock_ = -1;
    int maxConnections_ = 0;
    int idleTimeoutSec_ = 0;
    std::atomic<int> activeConnections_{0};
    std::atomic<bool> running_{false};
    std::thread acceptThread_;

    std::mutex clientsMutex_;
    std::vector<std::thread> clientThreads_;
    std::vector<int> clientSockets_;
};

} // namespace gateway

#endif // GATEWAY_SERVER_GATEWAY_IMPL_H
