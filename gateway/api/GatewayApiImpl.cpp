/**
  ******************************************************************************
  * @file           : GatewayApiImpl.cpp
  * @author         : vivi wu
  * @brief          : GatewayApi实现（ZMQ_DEALER）
  * @version        : 0.2.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#include "api/GatewayApiImpl.h"
#include "common/message_utils.h"
#include "common/gateway_msg_types.h"

// protobuf 相关头文件仅在实现文件中包含，不向业务层暴露
#include "gateway.pb.h"

#include <iostream>
#include <string>

#include <zmq.h>

namespace gateway {

// ============================================================================
// 工厂方法
// ============================================================================
GatewayApi* GatewayApi::CreateGatewayApi() {
    return new GatewayApiImpl();
}

// ============================================================================
// 辅助：业务 struct <=> protobuf Message 转换
// ============================================================================

static gateway_proto::LoginRequest ToProtoRequest(const gateway::LoginRequest& req) {
    gateway_proto::LoginRequest proto;
    proto.set_request_id(req.request_id);
    proto.set_username(req.username);
    proto.set_password(req.password);
    return proto;
}

static gateway::LoginResponse FromProtoResponse(const gateway_proto::LoginResponse& proto) {
    gateway::LoginResponse rsp;
    rsp.request_id = proto.request_id();
    rsp.error_code = proto.error_code();
    rsp.error_msg  = proto.error_msg();
    if (proto.has_account()) {
        rsp.account.user_id  = proto.account().user_id();
        rsp.account.username = proto.account().username();
        rsp.account.email    = proto.account().email();
        rsp.account.role     = proto.account().role();
    }
    return rsp;
}

// ============================================================================
// 构造/析构
// ============================================================================
GatewayApiImpl::GatewayApiImpl()
    : spi_(nullptr), dealerSock_(nullptr) {}

GatewayApiImpl::~GatewayApiImpl() {
    running_ = false;
    JoinThread();   // 先等待接收线程退出
    Disconnect();   // 再关闭 socket
}

// ============================================================================
// 生命周期
// ============================================================================
void GatewayApiImpl::Release() {
    delete this;
}

void GatewayApiImpl::RegisterSpi(GatewaySpi* pSpi) {
    spi_ = pSpi;
}

int GatewayApiImpl::Init(const char* frontAddress, int port, int heartbeatIntervalSec) {
    if (dealerSock_) {
        return ERR_OK;
    }

    // 如果之前有旧线程（如断线后未清理），先安全回收
    JoinThread();

    frontAddress_ = frontAddress ? frontAddress : "";
    port_ = port;
    heartbeatIntervalSec_ = heartbeatIntervalSec;

    void* sock = CreateDealerSocket();
    if (!sock) {
        std::cerr << "Failed to create ZMQ_DEALER socket\n";
        return ERR_NETWORK;
    }

    // 启用 TCP KeepAlive
    SetTcpKeepalive(sock, 1, heartbeatIntervalSec_, heartbeatIntervalSec_, 3);

    std::string address = "tcp://" + frontAddress_ + ":" + std::to_string(port_);
    if (!ConnectSocket(sock, address.c_str())) {
        CloseSocket(sock);
        return ERR_NETWORK;
    }

    dealerSock_ = sock;
    running_ = true;
    recvThread_ = std::thread(&GatewayApiImpl::RecvThreadFunc, this);

    if (spi_) {
        spi_->OnFrontConnected();
    }
    return ERR_OK;
}

int GatewayApiImpl::Login(const LoginRequest& req) {
    if (!dealerSock_) {
        std::cerr << "Not connected, call Init() first\n";
        return ERR_NETWORK;
    }

    // struct -> protobuf -> bytes
    gateway_proto::LoginRequest protoReq = ToProtoRequest(req);
    std::string reqData = protoReq.SerializeAsString();
    std::vector<uint8_t> body(reqData.begin(), reqData.end());

    if (!SendMessage(dealerSock_, static_cast<uint16_t>(GatewayMsgType::kLoginRequest), body)) {
        std::cerr << "Failed to send LoginRequest\n";
        return ERR_SEND_FAILED;
    }
    return ERR_OK;
}

// ============================================================================
// 后台接收线程
// ============================================================================
void GatewayApiImpl::RecvThreadFunc() {
    zmq_pollitem_t items[] = { { dealerSock_, 0, ZMQ_POLLIN, 0 } };

    while (running_.load()) {
        int rc = zmq_poll(items, 1, 100);  // 100ms 超时，确保能及时响应 running_ 变化
        if (rc < 0) {
            if (running_.load()) std::cerr << "zmq_poll error: " << zmq_strerror(zmq_errno()) << "\n";
            break;
        }
        if (rc == 0) continue;  // 超时，重新检查 running_

        if (!(items[0].revents & ZMQ_POLLIN)) continue;

        uint16_t rawType = 0;
        std::vector<uint8_t> body;
        if (!RecvMessage(dealerSock_, rawType, body)) {
            if (running_.load()) {
                std::cerr << "Recv error or server disconnected\n";
            }
            Disconnect();
            if (running_.load() && spi_) {
                spi_->OnFrontDisconnected(0);
            }
            break;
        }

        // 仅处理登录响应
        if (static_cast<GatewayMsgType>(rawType) != GatewayMsgType::kLoginResponse) {
            std::cerr << "Unexpected message type: " << rawType << "\n";
            continue;
        }

        // bytes -> protobuf -> struct -> callback
        gateway_proto::LoginResponse protoResp;
        if (!protoResp.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
            std::cerr << "Failed to parse response\n";
            continue;
        }

        gateway::LoginResponse rsp = FromProtoResponse(protoResp);

        if (spi_) {
            spi_->OnLogin(rsp);
        }
    }
}

void GatewayApiImpl::Disconnect() {
    if (dealerSock_) {
        CloseSocket(dealerSock_);
        dealerSock_ = nullptr;
    }
}

void GatewayApiImpl::JoinThread() {
    if (recvThread_.joinable()) {
        recvThread_.join();
    }
}

} // namespace gateway
