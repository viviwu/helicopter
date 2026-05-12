/**
 **************************************************************************
 * @file    :TcpServer.cpp
 * @author  :viviwu
 * @brief   :TcpServer实现
 * @version :0.1
 * @date    :5/12/26 PM3:39
 * **************************************************************************
 */

#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "SocketUtil.h"
#include "TcpConnection.h"
#include "../base/Logger.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

namespace gateway {

TcpServer::TcpServer(EventLoop* loop,
                     const struct sockaddr_in& listenAddr,
                     const std::string& name,
                     Option option)
    : loop_(loop),
      ipPort_(sockets::ToIpPort(listenAddr)),
      name_(name),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      threadPool_(new EventLoopThreadPool(loop, name)),
      connectionCallback_(TcpConnection::DefaultConnectionCallback),
      messageCallback_(TcpConnection::DefaultMessageCallback),
      nextConnId_(1) {
  acceptor_->SetNewConnectionCallback(
      std::bind(&TcpServer::NewConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer() {
  loop_->AssertInLoopThread();
  LOG_TRACE("TcpServer::~TcpServer [{}] destructing", name_);

  for (auto& item : connections_) {
    TcpConnection::TcpConnectionPtr conn(item.second);
    item.second.reset();
    conn->GetLoop()->RunInLoop(
        std::bind(&TcpConnection::ConnectDestroyed, conn));
  }
}

void TcpServer::SetThreadNum(int numThreads) {
  assert(0 <= numThreads);
  threadPool_->SetThreadNum(numThreads);
}

void TcpServer::Start() {
  if (started_.fetch_add(1) == 0) {
    threadPool_->Start();
    assert(!acceptor_->Listening());
    loop_->RunInLoop(std::bind(&Acceptor::Listen, acceptor_.get()));
  }
}

void TcpServer::NewConnection(int sockfd, const struct sockaddr_in& peerAddr) {
  loop_->AssertInLoopThread();
  EventLoop* ioLoop = threadPool_->GetNextLoop();
  char buf[64];
  snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  std::string connName = name_ + buf;

  LOG_INFO("TcpServer::NewConnection [{}] - new connection [{}] from {}",
           name_, connName, sockets::ToIpPort(peerAddr));

  struct sockaddr_in localAddr = sockets::GetLocalAddr(sockfd);
  TcpConnection::TcpConnectionPtr conn(
      new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
  connections_[connName] = conn;
  conn->SetConnectionCallback(connectionCallback_);
  conn->SetMessageCallback(messageCallback_);
  conn->SetWriteCompleteCallback(writeCompleteCallback_);
  conn->SetCloseCallback(
      std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
  ioLoop->RunInLoop(std::bind(&TcpConnection::ConnectEstablished, conn));
}

void TcpServer::RemoveConnection(const TcpConnection::TcpConnectionPtr& conn) {
  loop_->RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
}

void TcpServer::RemoveConnectionInLoop(const TcpConnection::TcpConnectionPtr& conn) {
  loop_->AssertInLoopThread();
  LOG_INFO("TcpServer::RemoveConnectionInLoop [{}] - connection {}",
           name_, conn->Name());
  size_t n = connections_.erase(conn->Name());
  (void)n;
  assert(n == 1);
  EventLoop* ioLoop = conn->GetLoop();
  ioLoop->QueueInLoop(std::bind(&TcpConnection::ConnectDestroyed, conn));
}

}
