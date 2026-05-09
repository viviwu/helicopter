/**
  ******************************************************************************
  * @file           : GatewayApi.h
  * @author         : vivi wu
  * @brief          : Gateway API 与回调接口（业务层可见，不暴露 protobuf）
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#ifndef GATEWAY_API_H
#define GATEWAY_API_H

#include "api/GatewayApiDef.h"

namespace gateway {

// ============================================================================
// 回调接口 Spi（用户必须继承并实现）
// ============================================================================

class GatewaySpi {
public:
    virtual ~GatewaySpi() = default;

    /// 当客户端与交易网关建立起通信连接时，该方法被调用
    virtual void OnFrontConnected() {}

    /// 当客户端与交易网关通信连接断开时，该方法被调用
    /// @param nReason 断开原因，0-正常断开/网络错误
    virtual void OnFrontDisconnected(int nReason) {}

    /// 登录请求响应（异步回调）
    /// @param rsp 登录响应结构
    virtual void OnLogin(const LoginResponse& rsp) = 0;
};

// ============================================================================
// API 接口
// ============================================================================

class GatewayApi {
public:
    virtual ~GatewayApi() = default;

    /// 创建GatewayApi实例
    /// @return GatewayApi实例指针，不再使用时调用 Release 释放
    static GatewayApi* CreateGatewayApi();

    /// 删除接口对象本身；调用后不应再使用该指针
    virtual void Release() = 0;

    /// 注册回调接口实例
    /// @param pSpi 派生自 GatewaySpi 的用户实例
    virtual void RegisterSpi(GatewaySpi* pSpi) = 0;

    /// 初始化并建立到前置机的网络连接
    /// @param frontAddress 前置机地址，如 "127.0.0.1"
    /// @param port 前置机端口
    /// @param heartbeatIntervalSec TCP keepalive 空闲探测间隔（秒），同时也作为探测间隔
    /// @return 0-成功，ERR_NETWORK-连接失败
    virtual int Init(const char* frontAddress, int port, int heartbeatIntervalSec) = 0;

    /// 发起登录请求（异步，结果通过 GatewaySpi::OnLogin 回调）
    /// @param req 登录请求结构
    /// @return 0-发送成功，ERR_SEND_FAILED-发送失败，ERR_NETWORK-未连接
    virtual int Login(const LoginRequest& req) = 0;
};

} // namespace gateway

#endif // GATEWAY_API_H
