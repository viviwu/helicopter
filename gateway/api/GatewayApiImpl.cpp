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
    JoinHeartbeatThread();  // 先停止心跳线程
    JoinThread();           // 再等待接收线程退出
    Disconnect();           // 最后关闭 socket
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

    // 启用 ZMQ 内置心跳（替代 TCP keepalive，跨 NAT/代理更可靠）
    // ivl=3s, timeout=9s, ttl=20s
    SetZmqHeartbeat(sock, 3000, 9000, 20000);

    // 保留 TCP keepalive 作为底层兜底（部分旧版 ZMQ 可能不支持 ZMQ_HEARTBEAT）
    SetTcpKeepalive(sock, 1, heartbeatIntervalSec_, heartbeatIntervalSec_, 3);

    std::string address = "tcp://" + frontAddress_ + ":" + std::to_string(port_);
    if (!ConnectSocket(sock, address.c_str())) {
        CloseSocket(sock);
        return ERR_NETWORK;
    }

    dealerSock_ = sock;
    running_ = true;
    lastPongTime_ = std::chrono::steady_clock::now();
    missedPongs_ = 0;
    recvThread_ = std::thread(&GatewayApiImpl::RecvThreadFunc, this);
    heartbeatThread_ = std::thread(&GatewayApiImpl::HeartbeatThreadFunc, this);

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

        auto msgType = static_cast<GatewayMsgType>(rawType);

        // 处理应用层心跳 Pong
        if (msgType == GatewayMsgType::kPong) {
            lastPongTime_ = std::chrono::steady_clock::now();
            missedPongs_ = 0;
            continue;
        }

        // 仅处理登录响应
        if (msgType != GatewayMsgType::kLoginResponse) {
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

void GatewayApiImpl::HeartbeatThreadFunc() {
    while (running_.load()) {
        // 按心跳间隔休眠（分 100ms 片段检查 running_）
        int slept = 0;
        int targetMs = heartbeatIntervalSec_ * 1000;
        while (slept < targetMs && running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            slept += 100;
        }
        if (!running_.load()) break;

        // 发送应用层 Ping
        std::vector<uint8_t> emptyBody;
        if (!SendMessage(dealerSock_, static_cast<uint16_t>(GatewayMsgType::kPing), emptyBody)) {
            std::cerr << "Failed to send Ping, connection may be dead\n";
            int missed = missedPongs_.fetch_add(1) + 1;
            if (missed >= kMaxMissedPongs) {
                running_ = false;
                if (spi_) {
                    spi_->OnFrontDisconnected(1);  // 1=心跳超时
                }
                break;
            }
            continue;
        }

        // 检查上次收到 Pong 的时间
        auto now = std::chrono::steady_clock::now();
        auto lastPong = lastPongTime_.load();
        auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - lastPong).count();
        int threshold = heartbeatIntervalSec_ * kMaxMissedPongs;
        if (elapsedSec >= threshold) {
            std::cerr << "Heartbeat timeout (no Pong for " << elapsedSec << "s)\n";
            running_ = false;
            if (spi_) {
                spi_->OnFrontDisconnected(1);  // 1=心跳超时
            }
            break;
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

void GatewayApiImpl::JoinHeartbeatThread() {
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
}

} // namespace gateway
