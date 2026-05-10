/**
  ******************************************************************************
  * @file           : GatewayImpl.h
  * @author         : vivi wu
  * @brief          : Gateway 服务端实现（ZMQ_ROUTER / 消息收发 / protobuf 解析归档）
  * @version        : 0.2.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#ifndef GATEWAY_SERVER_GATEWAY_IMPL_H
#define GATEWAY_SERVER_GATEWAY_IMPL_H

#include "server/Gateway.h"
#include "common/ThreadPool.h"

#include <atomic>
#include <chrono>
#include <map>
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
    void BroadcastNotification(const std::string& content) override;

private:
    void DispatchThreadFunc();
    void ProcessMessage(const std::vector<uint8_t>& identity,
                        uint16_t rawType, const std::vector<uint8_t>& body);
    void SendPong(const std::vector<uint8_t>& identity);
    void CleanupIdleConnections();

    using TimePoint = std::chrono::steady_clock::time_point;

    IGatewayHandler* handler_ = nullptr;
    ThreadPool* threadPool_ = nullptr;
    void* routerSock_ = nullptr;
    void* pubSock_ = nullptr;
    std::string bindAddr_;
    int routerPort_ = 0;
    int maxConnections_ = 0;
    int idleTimeoutSec_ = 0;
    std::atomic<int> activeConnections_{0};
    std::atomic<bool> running_{false};
    std::thread dispatchThread_;

    // 连接活跃时间管理（identity -> 最后活跃时间）
    std::mutex connMutex_;
    std::map<std::vector<uint8_t>, TimePoint> lastActive_;
};

} // namespace gateway

#endif // GATEWAY_SERVER_GATEWAY_IMPL_H
