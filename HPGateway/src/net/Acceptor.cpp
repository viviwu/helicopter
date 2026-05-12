/**
 **************************************************************************
 * @file    :Acceptor.cpp
 * @author  :viviwu
 * @brief   :Acceptor实现
 * @version :0.1
 * @date    :5/12/26 PM3:37
 * **************************************************************************
 */

#include "Acceptor.h"
#include "EventLoop.h"
#include "Channel.h"
#include "SocketUtil.h"
#include "../base/Logger.h"
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

namespace gateway {

Acceptor::Acceptor(EventLoop* loop, const struct sockaddr_in& listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(sockets::CreateNonblockingOrDie()),
      acceptChannel_(new Channel(loop, acceptSocket_)),
      listening_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
  assert(idleFd_ >= 0);
  sockets::SetReuseAddr(acceptSocket_, true);
  sockets::SetReusePort(acceptSocket_, reuseport);
  sockets::BindOrDie(acceptSocket_, listenAddr);
  acceptChannel_->SetReadCallback(std::bind(&Acceptor::HandleRead, this));
}

Acceptor::~Acceptor() {
  acceptChannel_->DisableAll();
  acceptChannel_->Remove();
  ::close(idleFd_);
  sockets::Close(acceptSocket_);
}

void Acceptor::Listen() {
  loop_->AssertInLoopThread();
  listening_ = true;
  sockets::ListenOrDie(acceptSocket_);
  acceptChannel_->EnableReading();
}

void Acceptor::HandleRead() {
  loop_->AssertInLoopThread();
  struct sockaddr_in peerAddr;
  memset(&peerAddr, 0, sizeof(peerAddr));
  int connfd = sockets::Accept(acceptSocket_, &peerAddr);
  if (connfd >= 0) {
    if (newConnectionCallback_) {
      newConnectionCallback_(connfd, peerAddr);
    } else {
      sockets::Close(connfd);
    }
  } else {
    LOG_ERROR("in Acceptor::HandleRead");
    if (errno == EMFILE) {
      ::close(idleFd_);
      idleFd_ = ::accept(acceptSocket_, NULL, NULL);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}

}
