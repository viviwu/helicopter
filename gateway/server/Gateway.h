/**
  ******************************************************************************
  * @file           : Gateway.h
  * @author         : vivi wu
  * @brief          : Gateway 服务端框架接口（TCP/解析/归档 + 业务 handler 注入）
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#ifndef GATEWAY_SERVER_GATEWAY_H
#define GATEWAY_SERVER_GATEWAY_H

#include "common/gateway_msg_types.h"

#include <string>

class ThreadPool;  // forward declare

namespace gateway_proto {
class LoginRequest;
class LoginResponse;
}

namespace gateway {

// ============================================================================
// 业务处理接口（业务模块实现此接口并注入 Gateway）
// ============================================================================
class IGatewayHandler {
public:
    virtual ~IGatewayHandler() = default;

    /// 处理登录请求
    /// @return true-处理成功并已填充响应, false-处理失败
    virtual bool OnLogin(const gateway_proto::LoginRequest& req,
                         gateway_proto::LoginResponse& rsp) = 0;

    // 后续拓展:
    // virtual bool OnQueryOrder(const gateway_proto::QueryOrderRequest& req,
    //                           gateway_proto::QueryOrderResponse& rsp) { return false; }
};

// ============================================================================
// Gateway 服务端抽象接口
// ============================================================================
class Gateway {
public:
    /// 创建 Gateway 实例
    static Gateway* Create();

    virtual ~Gateway() = default;

    /// 销毁实例
    virtual void Release() = 0;

    /// 注册业务处理器（必须在 Start 之前调用）
    virtual void RegisterHandler(IGatewayHandler* handler) = 0;

    /// 设置线程池（必须在 Start 之前调用，可选）
    /// 若未设置，每个客户端连接将创建独立线程
    virtual void SetThreadPool(ThreadPool* pool) = 0;

    /// 设置最大并发连接数（必须在 Start 之前调用，可选）
    /// @param max 最大连接数，0 表示不限制
    virtual void SetMaxConnections(int max) = 0;

    /// 设置连接空闲超时（必须在 Start 之前调用，可选）
    /// 超时后连接自动关闭回收，0 表示不启用
    /// @param seconds 空闲超时秒数
    virtual void SetIdleTimeout(int seconds) = 0;

    /// 启动 TCP 服务（同时启动 ROUTER 和 PUB socket）
    /// @param bindAddr 绑定地址，如 "0.0.0.0"
    /// @param port 监听端口（PUB 端口固定为 port + 1）
    /// @return true-启动成功, false-失败
    virtual bool Start(const char* bindAddr, int port) = 0;

    /// 停止服务
    virtual void Stop() = 0;

    /// 向所有已连接的客户端广播通知（通过 PUB socket）
    /// @param content 通知内容
    virtual void BroadcastNotification(const std::string& content) = 0;
};

} // namespace gateway

#endif // GATEWAY_SERVER_GATEWAY_H
