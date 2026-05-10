/**
  ******************************************************************************
  * @file           : message_utils.cpp
  * @author         : vivi wu
  * @brief          : ZeroMQ 多部分消息收发包实现
  * @version        : 0.3.0
  * @date           : 09/05/26
  ******************************************************************************
  */

#include "message_utils.h"

#include <zmq.h>

#include <cstring>
#include <iostream>
#include <mutex>

// ============================================================================
// ZMQ Context 单例
// ============================================================================

static void* g_zmqCtx = nullptr;
static std::once_flag g_zmqCtxFlag;

void* GetZmqContext() {
    std::call_once(g_zmqCtxFlag, [] {
        g_zmqCtx = zmq_ctx_new();
        if (!g_zmqCtx) {
            std::cerr << "Failed to create ZMQ context\n";
        }
    });
    return g_zmqCtx;
}

// ============================================================================
// Socket 生命周期
// ============================================================================

void* CreateRouterSocket() {
    void* sock = zmq_socket(GetZmqContext(), ZMQ_ROUTER);
    if (sock) {
        int linger = 0;
        zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(linger));
    }
    return sock;
}

void* CreateDealerSocket() {
    void* sock = zmq_socket(GetZmqContext(), ZMQ_DEALER);
    if (sock) {
        int linger = 0;
        zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(linger));
    }
    return sock;
}

void CloseSocket(void* sock) {
    if (sock) {
        zmq_close(sock);
    }
}

// ============================================================================
// Socket 选项
// ============================================================================

void SetLinger(void* sock, int ms) {
    zmq_setsockopt(sock, ZMQ_LINGER, &ms, sizeof(ms));
}

void SetRecvTimeout(void* sock, int ms) {
    zmq_setsockopt(sock, ZMQ_RCVTIMEO, &ms, sizeof(ms));
}

void SetTcpKeepalive(void* sock, int keepalive, int idle, int interval, int count) {
    zmq_setsockopt(sock, ZMQ_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
    zmq_setsockopt(sock, ZMQ_TCP_KEEPALIVE_IDLE, &idle, sizeof(idle));
    zmq_setsockopt(sock, ZMQ_TCP_KEEPALIVE_INTVL, &interval, sizeof(interval));
    zmq_setsockopt(sock, ZMQ_TCP_KEEPALIVE_CNT, &count, sizeof(count));
}

void SetZmqHeartbeat(void* sock, int ivlMs, int timeoutMs, int ttlMs) {
    zmq_setsockopt(sock, ZMQ_HEARTBEAT_IVL, &ivlMs, sizeof(ivlMs));
    zmq_setsockopt(sock, ZMQ_HEARTBEAT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    zmq_setsockopt(sock, ZMQ_HEARTBEAT_TTL, &ttlMs, sizeof(ttlMs));
}

// ============================================================================
// 绑定 / 连接
// ============================================================================

bool BindSocket(void* sock, const char* address) {
    if (zmq_bind(sock, address) != 0) {
        std::cerr << "zmq_bind failed: " << zmq_strerror(zmq_errno()) << "\n";
        return false;
    }
    return true;
}

bool ConnectSocket(void* sock, const char* address) {
    if (zmq_connect(sock, address) != 0) {
        std::cerr << "zmq_connect failed: " << zmq_strerror(zmq_errno()) << "\n";
        return false;
    }
    return true;
}

// ============================================================================
// 辅助：接收单帧
// ============================================================================

static bool RecvFrame(void* sock, zmq_msg_t& msg) {
    if (zmq_msg_init(&msg) != 0) return false;
    int rc = zmq_msg_recv(&msg, sock, 0);
    if (rc < 0) {
        zmq_msg_close(&msg);
        return false;
    }
    return true;
}

static bool RecvFrameToVector(void* sock, std::vector<uint8_t>& out) {
    zmq_msg_t msg;
    if (!RecvFrame(sock, msg)) return false;
    size_t size = zmq_msg_size(&msg);
    auto* data = static_cast<const uint8_t*>(zmq_msg_data(&msg));
    out.assign(data, data + size);
    zmq_msg_close(&msg);
    return true;
}

// ============================================================================
// 辅助：发送单帧
// ============================================================================

static bool SendFrame(void* sock, const void* data, size_t size, bool more) {
    zmq_msg_t msg;
    if (zmq_msg_init_size(&msg, size) != 0) return false;
    if (size > 0) {
        std::memcpy(zmq_msg_data(&msg), data, size);
    }
    int flags = more ? ZMQ_SNDMORE : 0;
    int rc = zmq_msg_send(&msg, sock, flags);
    zmq_msg_close(&msg);
    return rc >= 0;
}

// ============================================================================
// DEALER 端收发（2 帧：msg_type + body）
// ============================================================================

bool RecvMessage(void* sock, uint16_t& outMsgType, std::vector<uint8_t>& outBody) {
    // Frame 0: msg_type
    std::vector<uint8_t> typeFrame;
    if (!RecvFrameToVector(sock, typeFrame)) return false;
    if (typeFrame.size() < sizeof(uint16_t)) {
        std::cerr << "RecvMessage: invalid msg_type frame size=" << typeFrame.size() << "\n";
        return false;
    }
    std::memcpy(&outMsgType, typeFrame.data(), sizeof(uint16_t));

    // Check for more frames
    int more = 0;
    size_t moreSz = sizeof(more);
    zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &moreSz);
    if (!more) {
        std::cerr << "RecvMessage: missing body frame\n";
        return false;
    }

    // Frame 1: body
    if (!RecvFrameToVector(sock, outBody)) return false;

    return true;
}

bool SendMessage(void* sock, uint16_t msgType, const std::vector<uint8_t>& body) {
    // Frame 0: msg_type (with SNDMORE)
    if (!SendFrame(sock, &msgType, sizeof(msgType), true)) return false;

    // Frame 1: body (last frame)
    if (!SendFrame(sock, body.data(), body.size(), false)) return false;

    return true;
}

// ============================================================================
// ROUTER 端收发（3 帧：identity + msg_type + body）
// ============================================================================

bool RecvRouterMessage(void* sock, std::vector<uint8_t>& outIdentity,
                       uint16_t& outMsgType, std::vector<uint8_t>& outBody) {
    // Frame 0: identity
    if (!RecvFrameToVector(sock, outIdentity)) return false;

    // Frame 1: msg_type
    int more = 0;
    size_t moreSz = sizeof(more);
    zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &moreSz);
    if (!more) {
        std::cerr << "RecvRouterMessage: missing msg_type frame\n";
        return false;
    }

    std::vector<uint8_t> typeFrame;
    if (!RecvFrameToVector(sock, typeFrame)) return false;
    if (typeFrame.size() < sizeof(uint16_t)) {
        std::cerr << "RecvRouterMessage: invalid msg_type frame size=" << typeFrame.size() << "\n";
        return false;
    }
    std::memcpy(&outMsgType, typeFrame.data(), sizeof(uint16_t));

    // Frame 2: body
    zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &moreSz);
    if (!more) {
        std::cerr << "RecvRouterMessage: missing body frame\n";
        return false;
    }

    if (!RecvFrameToVector(sock, outBody)) return false;

    return true;
}

bool SendRouterMessage(void* sock, const std::vector<uint8_t>& identity,
                       uint16_t msgType, const std::vector<uint8_t>& body) {
    // Frame 0: identity (with SNDMORE)
    if (!SendFrame(sock, identity.data(), identity.size(), true)) return false;

    // Frame 1: msg_type (with SNDMORE)
    if (!SendFrame(sock, &msgType, sizeof(msgType), true)) return false;

    // Frame 2: body (last frame)
    if (!SendFrame(sock, body.data(), body.size(), false)) return false;

    return true;
}
