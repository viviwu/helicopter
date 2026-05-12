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
#include "../net/SocketUtil.h"
#include "../protocol/Codec.h"
#include "../base/ThreadPool.h"
#include "../base/Logger.h"
#include <assert.h>

namespace gateway {

GatewayServer::GatewayServer(EventLoop* loop, const struct sockaddr_in& listenAddr)
    : loop_(loop),
      server_(new TcpServer(loop, listenAddr, "GatewayServer")),
      codec_(new Codec(std::bind(&GatewayServer::OnPacket, this, std::placeholders::_1, std::placeholders::_2))),
      workerPool_(new ThreadPool("WorkerPool")),
      spi_(nullptr),
      started_(false) {
  server_->SetConnectionCallback(
      std::bind(&GatewayServer::OnConnection, this, std::placeholders::_1));
  server_->SetMessageCallback(
      std::bind(&GatewayServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  server_->SetWriteCompleteCallback(
      std::bind(&GatewayServer::OnWriteComplete, this, std::placeholders::_1));
}

GatewayServer::~GatewayServer() {
  Stop();
}

void GatewayServer::SetThreadNum(int numThreads) {
  server_->SetThreadNum(numThreads);
}

void GatewayServer::SetWorkerThreadNum(int numThreads) {
  workerPool_->SetThreadNum(numThreads);
}

void GatewayServer::Start() {
  assert(!started_.load());
  started_ = true;
  workerPool_->Start();
  server_->Start();
  LOG_INFO("GatewayServer started");
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
    SessionManager::Instance().AddSession(connId);
  } else {
    LOG_INFO("GatewayServer::OnConnection DOWN - {} -> {}",
             sockets::ToIpPort(conn->LocalAddress()),
             sockets::ToIpPort(conn->PeerAddress()));
  }
}

void GatewayServer::OnMessage(const std::shared_ptr<TcpConnection>& conn,
                              RingBuffer* buf,
                              Timestamp receiveTime) {
  loop_->AssertInLoopThread();
  codec_->OnMessage(conn, buf, receiveTime);
}

void GatewayServer::OnWriteComplete(const std::shared_ptr<TcpConnection>& conn) {
  LOG_TRACE("GatewayServer::OnWriteComplete {}", conn->Name());
}

void GatewayServer::OnPacket(const Packet& packet, int64_t connId) {
  workerPool_->Run(std::bind(&PacketDispatcher::Dispatch, &dispatcher_, packet, connId));
  if (spi_) {
    spi_->OnMessage(&packet, static_cast<int>(connId));
  }
}

}
