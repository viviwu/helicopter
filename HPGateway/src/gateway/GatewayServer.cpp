/**
 **************************************************************************
 * @file    :GatewayServer.cpp
 * @author  :viviwu
 * @brief   :网关服务器实现
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#include "GatewayServer.h"
#include "GatewayContext.h"
#include "GatewayApiImpl.h"
#include "SessionManager.h"
#include "ConnectionManager.h"
#include "../net/TcpServer.h"
#include "../net/TcpConnection.h"
#include "../net/EventLoop.h"
#include "../net/Channel.h"
#include "../net/SocketUtil.h"
#include "../protocol/Codec.h"
#include "../base/ThreadPool.h"
#include "../base/Logger.h"
#include <cassert>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/timerfd.h>

namespace gateway {

static int CreateTimerFd(int intervalSec) {
  int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (fd < 0) {
    LOG_FATAL("timerfd_create failed");
  }
  struct itimerspec ts;
  bzero(&ts, sizeof(ts));
  ts.it_value.tv_sec = intervalSec;
  ts.it_value.tv_nsec = 0;
  ts.it_interval.tv_sec = intervalSec;
  ts.it_interval.tv_nsec = 0;
  if (::timerfd_settime(fd, 0, &ts, NULL) < 0) {
    LOG_FATAL("timerfd_settime failed");
  }
  return fd;
}

GatewayServer::GatewayServer(EventLoop* loop, const struct sockaddr_in& listenAddr)
    : loop_(loop),
      server_(new TcpServer(loop, listenAddr, "GatewayServer")),
      codec_(new Codec(std::bind(&GatewayServer::OnPacket, this,
                                  std::placeholders::_1, std::placeholders::_2))),
      workerPool_(new ThreadPool("WorkerPool")),
      spi_(nullptr),
      started_(false),
      heartbeatTimerFd_(-1),
      heartbeatTimeoutSec_(60) {
  server_->SetConnectionCallback(
      std::bind(&GatewayServer::OnConnection, this, std::placeholders::_1));
  server_->SetMessageCallback(
      std::bind(&GatewayServer::OnMessage, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
  server_->SetWriteCompleteCallback(
      std::bind(&GatewayServer::OnWriteComplete, this, std::placeholders::_1));

  const GatewayConfig& cfg = GatewayContext::Instance().Config();
  heartbeatTimeoutSec_ = cfg.heartbeatTimeout;
}

GatewayServer::~GatewayServer() {
  Stop();
  if (heartbeatChannel_) {
    heartbeatChannel_->DisableAll();
    heartbeatChannel_->Remove();
  }
  if (heartbeatTimerFd_ >= 0) {
    ::close(heartbeatTimerFd_);
  }
}

void GatewayServer::SetThreadNum(int numThreads) {
  server_->SetThreadNum(numThreads);
}

void GatewayServer::SetWorkerThreadNum(int numThreads) {
  workerPool_->SetThreadNum(numThreads);
}

void GatewayServer::SetupHeartbeat() {
  heartbeatTimerFd_ = CreateTimerFd(heartbeatTimeoutSec_ / 2);
  heartbeatChannel_.reset(new Channel(loop_, heartbeatTimerFd_));
  heartbeatChannel_->SetReadCallback(
      std::bind(&GatewayServer::OnHeartbeat, this, std::placeholders::_1));
  heartbeatChannel_->EnableReading();
}

void GatewayServer::OnHeartbeat(Timestamp receiveTime) {
  loop_->AssertInLoopThread();
  // 读出 timerfd 事件计数
  uint64_t expirations = 0;
  ssize_t n = ::read(heartbeatTimerFd_, &expirations, sizeof(expirations));
  if (n != sizeof(expirations)) {
    LOG_ERROR("GatewayServer::OnHeartbeat read {} bytes", n);
  }

  int64_t now = Timestamp::Now().MilliSecondsSinceEpoch();
  int64_t timeoutMs = static_cast<int64_t>(heartbeatTimeoutSec_) * 1000;

  for (auto it = lastHeartbeats_.begin(); it != lastHeartbeats_.end(); ) {
    if (now - it->second > timeoutMs) {
      LOG_INFO("Heartbeat timeout connId={} lastHb={} now={}", it->first, it->second, now);
      auto conn = ConnectionManager::Instance().GetConnection(it->first);
      if (conn) {
        conn->ForceClose();
      }
      SessionManager::Instance().RemoveSession(it->first);
      it = lastHeartbeats_.erase(it);
    } else {
      ++it;
    }
  }
}

void GatewayServer::Start() {
  assert(!started_.load());
  started_ = true;
  workerPool_->Start();
  SetupHeartbeat();
  server_->Start();
  LOG_INFO("GatewayServer started, heartbeat every {}s, timeout {}s",
           heartbeatTimeoutSec_ / 2, heartbeatTimeoutSec_);
}

void GatewayServer::Stop() {
  if (started_.exchange(false)) {
    workerPool_->Stop();
    loop_->Quit();
  }
}

void GatewayServer::OnConnection(const std::shared_ptr<TcpConnection>& conn) {
  loop_->AssertInLoopThread();
  if (conn->Connected()) {
    LOG_INFO("GatewayServer::OnConnection UP - {} -> {}",
             sockets::ToIpPort(conn->LocalAddress()),
             sockets::ToIpPort(conn->PeerAddress()));
    int64_t connId = ConnectionManager::Instance().AddConnection(conn);
    conn->SetConnId(connId);
    SessionManager::Instance().AddSession(connId);

    lastHeartbeats_[connId] = Timestamp::Now().MilliSecondsSinceEpoch();
  } else {
    LOG_INFO("GatewayServer::OnConnection DOWN - {} -> {}",
             sockets::ToIpPort(conn->LocalAddress()),
             sockets::ToIpPort(conn->PeerAddress()));
    int64_t connId = conn->GetConnId();
    if (connId > 0) {
      ConnectionManager::Instance().RemoveConnection(connId);
      SessionManager::Instance().RemoveSession(connId);
      lastHeartbeats_.erase(connId);
    }
  }
}

void GatewayServer::OnMessage(const std::shared_ptr<TcpConnection>& conn,
                              RingBuffer* buf,
                              Timestamp receiveTime) {
  loop_->AssertInLoopThread();

  // 更新心跳时间
  int64_t connId = conn->GetConnId();
  if (connId > 0) {
    lastHeartbeats_[connId] = Timestamp::Now().MilliSecondsSinceEpoch();
  }

  codec_->OnMessage(conn, buf, receiveTime);
}

void GatewayServer::OnWriteComplete(const std::shared_ptr<TcpConnection>& conn) {
  LOG_TRACE("GatewayServer::OnWriteComplete {}", conn->Name());
}

void GatewayServer::OnPacket(const Packet& packet, int64_t connId) {
  // 处理心跳请求
  if (packet.Cmd() == kCmdHeartbeatReq) {
    LOG_TRACE("HeartbeatReq from connId={}", connId);
    Packet rsp;
    rsp.header.length = PacketHeader::kHeaderLength;
    rsp.header.cmd = kCmdHeartbeatRsp;
    rsp.header.flag = 0;
    rsp.header.seq = packet.header.seq;
    ConnectionManager::Instance().SendToClient(connId, &rsp, rsp.TotalLength());
    return;
  }

  workerPool_->Run(std::bind(&PacketDispatcher::Dispatch, &dispatcher_, packet, connId));
  if (spi_) {
    spi_->OnMessage(&packet, static_cast<int>(connId));
  }
}

}
