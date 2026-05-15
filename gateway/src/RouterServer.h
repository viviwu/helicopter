/**
  ******************************************************************************
  * @file           : RouterServer.h
  * @author         : vivi wu
  * @brief          : ROUTER 模式服务端基类（ZMQ_ROUTER / 消息分派 / 连接管理）
  * @version        : 0.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#ifndef FRAMEWORK_ROUTER_SERVER_H
#define FRAMEWORK_ROUTER_SERVER_H

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class ThreadPool;

namespace framework {

/// ROUTER 模式服务端基类
/// 提供 ZMQ_ROUTER 生命周期、dispatch 循环、连接管理、心跳处理。
/// 派生类实现 OnMessage() 完成业务消息分派。
class RouterServer {
public:
    RouterServer() = default;
    virtual ~RouterServer();

    // ==========================================================================
    // 生命周期
    // ==========================================================================

    /// 启动 ROUTER socket，绑定到 bindAddr:port，启动 dispatch 线程
    bool Start(const char* bindAddr, int port);
    void Stop();

    // ==========================================================================
    // 配置（必须在 Start 之前调用）
    // ==========================================================================

    void SetThreadPool(ThreadPool* pool)   { threadPool_ = pool; }
    void SetMaxConnections(int max)        { maxConnections_ = max; }
    void SetIdleTimeout(int seconds)       { idleTimeoutSec_ = seconds; }

    /// 获取当前活跃连接数
    int GetActiveConnections() const       { return activeConnections_.load(); }

protected:
    // ==========================================================================
    // 派生类必须实现
    // ==========================================================================

    /// 消息分派回调（在 ThreadPool 工作线程中执行）
    /// @param identity  客户端路由标识
    /// @param msgType   消息类型（派生类自定义枚举）
    /// @param body      消息体（protobuf 序列化字节）
    virtual void OnMessage(const std::vector<uint8_t>& identity,
                           uint16_t msgType,
                           const std::vector<uint8_t>& body) = 0;

    // ==========================================================================
    // 派生类可用
    // ==========================================================================

    /// 向指定客户端发送回复（3 帧：identity + msg_type + body）
    bool SendReply(const std::vector<uint8_t>& identity,
                   uint16_t msgType, const std::vector<uint8_t>& body);

    /// 检查是否正在运行
    bool IsRunning() const { return running_.load(); }

private:
    void DispatchLoop();
    void HandlePing(const std::vector<uint8_t>& identity);
    void CleanupIdleConnections();

    using TimePoint = std::chrono::steady_clock::time_point;

    ThreadPool* threadPool_ = nullptr;
    void* routerSock_ = nullptr;
    int maxConnections_ = 0;
    int idleTimeoutSec_ = 0;
    std::atomic<int> activeConnections_{0};
    std::atomic<bool> running_{false};
    std::thread dispatchThread_;

    std::mutex connMutex_;
    std::map<std::vector<uint8_t>, TimePoint> lastActive_;
};

} // namespace framework

#endif // FRAMEWORK_ROUTER_SERVER_H
