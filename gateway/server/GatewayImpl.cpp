/**
  ******************************************************************************
  * @file           : GatewayImpl.cpp
  * @author         : vivi wu
  * @brief          : Gateway 服务端实现
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#include "server/GatewayImpl.h"
#include "common/message_utils.h"
#include "gateway.pb.h"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace gateway {

// ============================================================================
// 工厂方法
// ============================================================================
Gateway* Gateway::Create() {
    return new GatewayImpl();
}

// ============================================================================
// 构造/析构
// ============================================================================
GatewayImpl::GatewayImpl() = default;

GatewayImpl::~GatewayImpl() {
    Stop();
}

// ============================================================================
// 生命周期
// ============================================================================
void GatewayImpl::Release() {
    delete this;
}

void GatewayImpl::RegisterHandler(IGatewayHandler* handler) {
    handler_ = handler;
}

void GatewayImpl::SetThreadPool(ThreadPool* pool) {
    threadPool_ = pool;
}

void GatewayImpl::SetMaxConnections(int max) {
    maxConnections_ = max;
}

void GatewayImpl::SetIdleTimeout(int seconds) {
    idleTimeoutSec_ = seconds;
}

bool GatewayImpl::Start(const char* bindAddr, int port) {
    if (serverSock_ >= 0) {
        std::cerr << "Gateway already started\n";
        return false;
    }

    serverSock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock_ < 0) {
        perror("socket");
        return false;
    }

    int opt = 1;
    setsockopt(serverSock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, bindAddr, &addr.sin_addr) <= 0) {
        std::cerr << "Invalid bind address: " << bindAddr << "\n";
        close(serverSock_);
        serverSock_ = -1;
        return false;
    }

    if (bind(serverSock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(serverSock_);
        serverSock_ = -1;
        return false;
    }

    if (listen(serverSock_, SOMAXCONN) < 0) {
        perror("listen");
        close(serverSock_);
        serverSock_ = -1;
        return false;
    }

    std::cout << "Gateway listening on " << bindAddr << ":" << port << "\n";

    running_ = true;
    acceptThread_ = std::thread(&GatewayImpl::AcceptThreadFunc, this);
    return true;
}

void GatewayImpl::Stop() {
    running_ = false;

    // 关闭 server socket 以解除 accept 阻塞
    if (serverSock_ >= 0) {
        shutdown(serverSock_, SHUT_RDWR);
        close(serverSock_);
        serverSock_ = -1;
    }

    // 关闭所有 client socket 以解除 recv 阻塞
    CloseAllClientSockets();

    // 等待 accept 线程退出
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    if (threadPool_) {
        // 线程池模式：client 任务由池管理，调用者负责 pool->Shutdown()
        // running_ 已置 false，client 任务将在完成当前循环后退出
        // CloseAllClientSockets 已关闭所有 socket，阻塞中的 recv 会立即返回
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clientSockets_.clear();
    } else {
        // 直接线程模式：join 所有 client 线程
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& t : clientThreads_) {
            if (t.joinable()) {
                t.join();
            }
        }
        clientThreads_.clear();
        clientSockets_.clear();
    }
}

// ============================================================================
// Accept 线程
// ============================================================================
void GatewayImpl::AcceptThreadFunc() {
    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSock = accept(serverSock_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientSock < 0) {
            if (running_) {
                perror("accept");
            }
            continue;
        }

        // 连接数限制检查
        if (maxConnections_ > 0) {
            int cur = activeConnections_.fetch_add(1) + 1;
            if (cur > maxConnections_) {
                activeConnections_--;
                std::cerr << "Max connections (" << maxConnections_
                          << ") reached, rejecting " << inet_ntoa(clientAddr.sin_addr) << "\n";
                close(clientSock);
                continue;
            }
        } else {
            activeConnections_++;
        }

        std::cout << "New connection from " << inet_ntoa(clientAddr.sin_addr)
                  << " (active=" << activeConnections_.load() << ")\n";

        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clientSockets_.push_back(clientSock);
        }

        if (threadPool_) {
            threadPool_->Submit([this, clientSock] {
                ClientThreadFunc(clientSock);
            });
        } else {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clientThreads_.emplace_back(&GatewayImpl::ClientThreadFunc, this, clientSock);
        }
    }
}

// ============================================================================
// Client 线程（消息接收 → 类型判断 → 分发 handler → 回复发送）
// ============================================================================
void GatewayImpl::ClientThreadFunc(int clientSock) {
    // 设置 socket 接收超时，用于空闲连接回收
    if (idleTimeoutSec_ > 0) {
        struct timeval tv;
        tv.tv_sec  = idleTimeoutSec_;
        tv.tv_usec = 0;
        setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    while (running_) {
        uint16_t rawType = 0;
        std::vector<uint8_t> body;
        if (!recvMessage(clientSock, rawType, body)) {
            if (running_) {
                std::cerr << "Client disconnected or recv error\n";
            }
            break;
        }

        auto msgType = static_cast<GatewayMsgType>(rawType);

        // ---- 消息类型分发 ----
        switch (msgType) {
        case GatewayMsgType::kLoginRequest: {
            gateway_proto::LoginRequest req;
            if (!req.ParseFromArray(body.data(), static_cast<int>(body.size()))) {
                std::cerr << "Failed to parse LoginRequest\n";
                continue;
            }

            gateway_proto::LoginResponse rsp;
            if (handler_ && handler_->OnLogin(req, rsp)) {
                // protobuf → bytes → 发送
                std::string data = rsp.SerializeAsString();
                std::vector<uint8_t> respBody(data.begin(), data.end());
                sendMessage(clientSock,
                            static_cast<uint16_t>(GatewayMsgType::kLoginResponse),
                            respBody);
            }
            break;
        }
        // 后续拓展:
        // case GatewayMsgType::kQueryOrderRequest: { ... break; }
        // case GatewayMsgType::kSendOrderRequest:  { ... break; }
        default:
            std::cerr << "Unknown message type: " << rawType << "\n";
            break;
        }
    }

    // 仅当非 Stop() 关闭时主动 close socket（Stop 已通过 CloseAllClientSockets 关闭）
    if (running_) {
        close(clientSock);

        // 标记此 socket 已关闭，防止 Stop() 时重复 close
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (int& sock : clientSockets_) {
            if (sock == clientSock) {
                sock = -1;
                break;
            }
        }
    }

    activeConnections_--;
}

// ============================================================================
// 辅助：关闭所有客户端 socket
// ============================================================================
void GatewayImpl::CloseAllClientSockets() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (int& sock : clientSockets_) {
        if (sock >= 0) {
            close(sock);
            sock = -1;
        }
    }
}

} // namespace gateway
