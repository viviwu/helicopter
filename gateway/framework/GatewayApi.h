/**
  ******************************************************************************
  * @file           : GatewayApi.h
  * @author         : vivi wu
  * @brief          : DEALER+SUB 客户端基类（ZMQ_DEALER + ZMQ_SUB / 心跳 / 断线检测）
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef FRAMEWORK_GATEWAY_CLIENT_H
#define FRAMEWORK_GATEWAY_CLIENT_H

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace framework {

/// DEALER+SUB 客户端基类
/// 管理 ZMQ_DEALER（请求-应答）和 ZMQ_SUB（发布-订阅）连接，
/// 提供接收循环、心跳、断线检测。
class GatewayApi {
public:
    GatewayApi() = default;
    virtual ~GatewayApi();

    // ==========================================================================
    // 生命周期
    // ==========================================================================

    /// 连接到服务端
    /// @param address        服务端地址
    /// @param routerPort     ROUTER 端口（DEALER 连接），0 = 不需要 DEALER（纯 SUB 模式）
    /// @param pubPort        PUB 端口（SUB 连接），0 = 不需要 SUB（纯 DEALER 模式）
    /// @param heartbeatSec   应用层心跳间隔（秒），纯 SUB 模式忽略
    /// @return 0-成功，-1-失败
    int Connect(const char* address, int routerPort, int pubPort, int heartbeatSec);

    /// 断开连接
    void Disconnect();

    bool IsConnected() const { return connected_.load(); }

    // ==========================================================================
    // SUB 主题管理（纯 SUB 模式或双通道模式使用）
    // ==========================================================================

    /// 订阅 PUB 主题
    bool Subscribe(const std::string& topic);

    /// 取消订阅 PUB 主题
    bool Unsubscribe(const std::string& topic);

protected:
    // ==========================================================================
    // 派生类必须实现
    // ==========================================================================

    virtual void OnConnected() = 0;
    virtual void OnDisconnected(int reason) = 0;

    /// DEALER socket 收到消息（请求-应答通道）
    virtual void OnRouterMessage(uint16_t msgType,
                                 const std::vector<uint8_t>& body) = 0;

    /// SUB socket 收到消息（发布-订阅通道）
    virtual void OnPubMessage(const std::string& topic, uint16_t msgType,
                              const std::vector<uint8_t>& body) = 0;

    // ==========================================================================
    // 派生类可用
    // ==========================================================================

    /// 通过 DEALER socket 发送消息
    bool SendToRouter(uint16_t msgType, const std::vector<uint8_t>& body);

private:
    void RecvLoop();
    void HeartbeatLoop();

    std::string address_;
    int routerPort_ = 0;
    int pubPort_ = 0;
    int heartbeatSec_ = 5;
    void* dealerSock_ = nullptr;
    void* subSock_ = nullptr;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread recvThread_;
    std::thread heartbeatThread_;

    // 应用层心跳超时检测
    using TimePoint = std::chrono::steady_clock::time_point;
    std::atomic<TimePoint> lastPongTime_{TimePoint{}};
    std::atomic<int> missedPongs_{0};
    static constexpr int kMaxMissedPongs = 3;
};

} // namespace framework

#endif // FRAMEWORK_GATEWAY_CLIENT_H
