/**
  ******************************************************************************
  * @file           : GatewayApiImpl.cpp
  * @author         : vivi wu
  * @brief          : GatewayApi实现
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#include "api/GatewayApiImpl.h"
#include "common/message_utils.h"
#include "common/gateway_msg_types.h"

// protobuf 相关头文件仅在实现文件中包含，不向业务层暴露
#include "gateway.pb.h"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace gateway {

// ============================================================================
// 工厂方法
// ============================================================================
GatewayApi* GatewayApi::CreateGatewayApi() {
    return new GatewayApiImpl();
}

// ============================================================================
// 辅助：TCP KeepAlive（跨平台）
// ============================================================================
static void SetTcpKeepAlive(int sockfd, int heartbeatIntervalSec) {
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0) {
        perror("setsockopt SO_KEEPALIVE");
        return;
    }

    // 连接空闲 heartbeatIntervalSec 秒后开始发送探测包
#if defined(TCP_KEEPIDLE)
    int idle = heartbeatIntervalSec;
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
#elif defined(TCP_KEEPALIVE)
    int idle = heartbeatIntervalSec;
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle));
#endif

    // 探测间隔使用用户传入的心跳间隔
#if defined(TCP_KEEPINTVL)
    int interval = heartbeatIntervalSec;
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
#endif

    // 连续 3 次探测无响应则断开
#if defined(TCP_KEEPCNT)
    int count = 3;
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
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
    : spi_(nullptr), sockfd_(-1) {}

GatewayApiImpl::~GatewayApiImpl() {
    running_ = false;
    Disconnect();
    JoinThread();
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
    if (sockfd_ >= 0) {
        return ERR_OK;
    }

    // 如果之前有旧线程（如断线后未清理），先安全回收
    if (recvThread_.joinable()) {
        recvThread_.join();
    }

    frontAddress_ = frontAddress ? frontAddress : "";
    port_ = port;
    heartbeatIntervalSec_ = heartbeatIntervalSec;

    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        perror("socket");
        return ERR_NETWORK;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(static_cast<uint16_t>(port_));
    if (inet_pton(AF_INET, frontAddress_.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid frontAddress: " << frontAddress_ << "\n";
        close(sockfd_);
        sockfd_ = -1;
        return ERR_NETWORK;
    }

    if (connect(sockfd_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        perror("connect");
        close(sockfd_);
        sockfd_ = -1;
        return ERR_NETWORK;
    }

    // 启用 TCP keepalive，确保服务端异常/网络断开时能及时感知
    SetTcpKeepAlive(sockfd_, heartbeatIntervalSec_);

    running_ = true;
    recvThread_ = std::thread(&GatewayApiImpl::RecvThreadFunc, this);

    if (spi_) {
        spi_->OnFrontConnected();
    }
    return ERR_OK;
}

int GatewayApiImpl::Login(const LoginRequest& req) {
    if (sockfd_ < 0) {
        std::cerr << "Not connected, call Init() first\n";
        return ERR_NETWORK;
    }

    // struct -> protobuf -> bytes
    gateway_proto::LoginRequest protoReq = ToProtoRequest(req);
    std::string reqData = protoReq.SerializeAsString();
    std::vector<uint8_t> body(reqData.begin(), reqData.end());

    if (!sendMessage(sockfd_, static_cast<uint16_t>(GatewayMsgType::kLoginRequest), body)) {
        std::cerr << "Failed to send LoginRequest\n";
        return ERR_SEND_FAILED;
    }
    return ERR_OK;
}

// ============================================================================
// 后台线程
// ============================================================================
void GatewayApiImpl::RecvThreadFunc() {
    while (running_.load()) {
        uint16_t rawType = 0;
        std::vector<uint8_t> body;
        if (!recvMessage(sockfd_, rawType, body)) {
            // 先打印日志，再清理，再回调，避免回调里重入 Init 时看到旧 sockfd_
            if (running_.load()) {
                std::cerr << "Recv error or server disconnected\n";
            }
            Disconnect();  // 关闭 socket，置 sockfd_ = -1，支持下次重连
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
    if (sockfd_ >= 0) {
        shutdown(sockfd_, SHUT_RDWR);
        close(sockfd_);
        sockfd_ = -1;
    }
}

void GatewayApiImpl::JoinThread() {
    if (recvThread_.joinable()) {
        recvThread_.join();
    }
}

} // namespace gateway
