/**
 **************************************************************************
 * @file    :TcpConnection.cpp
 * @author  :viviwu
 * @brief   :TcpConnection实现
 * @version :0.1
 * @date    :5/12/26 PM3:38
 * **************************************************************************
 */

#include "TcpConnection.h"
#include "EventLoop.h"
#include "Channel.h"
#include "SocketUtil.h"
#include "../base/Logger.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

namespace gateway {

TcpConnection::TcpConnection(EventLoop* loop,
                             const std::string& name,
                             int sockfd,
                             const struct sockaddr_in& localAddr,
                             const struct sockaddr_in& peerAddr)
    : loop_(loop),
      name_(name),
      state_(kConnecting),
      reading_(true),
      socket_(sockfd),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024),
      connId_(0) {
  channel_->SetReadCallback(
      std::bind(&TcpConnection::HandleRead, this, std::placeholders::_1));
  channel_->SetWriteCallback(
      std::bind(&TcpConnection::HandleWrite, this));
  channel_->SetCloseCallback(
      std::bind(&TcpConnection::HandleClose, this));
  channel_->SetErrorCallback(
      std::bind(&TcpConnection::HandleError, this));
  LOG_DEBUG("TcpConnection::ctor[{}] at {} fd={}", name_, static_cast<const void*>(this), sockfd);
  sockets::SetKeepAlive(sockfd, true);
}

TcpConnection::~TcpConnection() {
  LOG_DEBUG("TcpConnection::dtor[{}] at {} fd={} state={}",
            name_, static_cast<const void*>(this), channel_->Fd(), StateToString());
  assert(state_ == kDisconnected);
}

void TcpConnection::Send(const void* data, int len) {
  Send(std::string(static_cast<const char*>(data), len));
}

void TcpConnection::Send(const std::string& message) {
  if (state_ == kConnected) {
    if (loop_->IsInLoopThread()) {
      SendInLoop(message);
    } else {
      void (TcpConnection::*fp)(const std::string& message) = &TcpConnection::SendInLoop;
      loop_->RunInLoop(
          std::bind(fp, this, message));
    }
  }
}

void TcpConnection::Send(RingBuffer* buf) {
  if (state_ == kConnected) {
    if (loop_->IsInLoopThread()) {
      SendInLoop(buf->Peek(), buf->ReadableBytes());
      buf->RetrieveAll();
    } else {
      void (TcpConnection::*fp)(const std::string& message) = &TcpConnection::SendInLoop;
      loop_->RunInLoop(
          std::bind(fp, this, buf->RetrieveAllAsString()));
    }
  }
}

void TcpConnection::SendInLoop(const std::string& message) {
  SendInLoop(message.data(), message.size());
}

void TcpConnection::SendInLoop(const void* data, size_t len) {
  loop_->AssertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool faultError = false;
  if (state_ == kDisconnected) {
    LOG_WARN("disconnected, give up writing");
    return;
  }
  if (!channel_->IsWriting() && outputBuffer_.ReadableBytes() == 0) {
    nwrote = ::write(channel_->Fd(), data, len);
    if (nwrote >= 0) {
      remaining = len - nwrote;
      if (remaining == 0 && writeCompleteCallback_) {
        loop_->QueueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
      }
    } else {
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        LOG_ERROR("TcpConnection::SendInLoop");
        if (errno == EPIPE || errno == ECONNRESET) {
          faultError = true;
        }
      }
    }
  }

  assert(remaining <= len);
  if (!faultError && remaining > 0) {
    size_t oldLen = outputBuffer_.ReadableBytes();
    if (oldLen + remaining >= highWaterMark_
        && oldLen < highWaterMark_
        && highWaterMarkCallback_) {
      loop_->QueueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
    }
    outputBuffer_.Append(static_cast<const char*>(data) + nwrote, remaining);
    if (!channel_->IsWriting()) {
      channel_->EnableWriting();
    }
  }
}

void TcpConnection::Shutdown() {
  if (state_ == kConnected) {
    SetState(kDisconnecting);
    loop_->RunInLoop(std::bind(&TcpConnection::ShutdownInLoop, this));
  }
}

void TcpConnection::ShutdownInLoop() {
  loop_->AssertInLoopThread();
  if (!channel_->IsWriting()) {
    sockets::ShutdownWrite(socket_);
  }
}

void TcpConnection::ForceClose() {
  if (state_ == kConnected || state_ == kDisconnecting) {
    SetState(kDisconnecting);
    loop_->QueueInLoop(std::bind(&TcpConnection::ForceCloseInLoop, shared_from_this()));
  }
}

void TcpConnection::ForceCloseInLoop() {
  loop_->AssertInLoopThread();
  if (state_ == kConnected || state_ == kDisconnecting) {
    HandleClose();
  }
}

void TcpConnection::SetTcpNoDelay(bool on) {
  sockets::SetTcpNoDelay(socket_, on);
}

void TcpConnection::ConnectEstablished() {
  loop_->AssertInLoopThread();
  assert(state_ == kConnecting);
  SetState(kConnected);
  channel_->EnableReading();

  connectionCallback_(shared_from_this());
}

void TcpConnection::ConnectDestroyed() {
  loop_->AssertInLoopThread();
  if (state_ == kConnected) {
    SetState(kDisconnected);
    channel_->DisableAll();
    connectionCallback_(shared_from_this());
  }
  channel_->Remove();
}

void TcpConnection::HandleRead(Timestamp receiveTime) {
  loop_->AssertInLoopThread();
  int savedErrno = 0;
  ssize_t n = inputBuffer_.ReadFd(channel_->Fd(), &savedErrno);
  if (n > 0) {
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  } else if (n == 0) {
    HandleClose();
  } else {
    errno = savedErrno;
    LOG_ERROR("TcpConnection::HandleRead");
    HandleError();
  }
}

void TcpConnection::HandleWrite() {
  loop_->AssertInLoopThread();
  if (channel_->IsWriting()) {
    ssize_t n = ::write(channel_->Fd(),
                        outputBuffer_.Peek(),
                        outputBuffer_.ReadableBytes());
    if (n > 0) {
      outputBuffer_.Retrieve(n);
      if (outputBuffer_.ReadableBytes() == 0) {
        channel_->DisableWriting();
        if (writeCompleteCallback_) {
          loop_->QueueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
        }
        if (state_ == kDisconnecting) {
          ShutdownInLoop();
        }
      }
    } else {
      LOG_ERROR("TcpConnection::HandleWrite");
    }
  } else {
    LOG_TRACE("Connection fd = {} is down, no more writing", channel_->Fd());
  }
}

void TcpConnection::HandleClose() {
  loop_->AssertInLoopThread();
  LOG_TRACE("fd = {} state = {}", channel_->Fd(), StateToString());
  assert(state_ == kConnected || state_ == kDisconnecting);
  SetState(kDisconnected);
  channel_->DisableAll();

  TcpConnectionPtr guardThis(shared_from_this());
  connectionCallback_(guardThis);
  closeCallback_(guardThis);
}

void TcpConnection::HandleError() {
  int err = sockets::GetSocketError(channel_->Fd());
  LOG_ERROR("TcpConnection::HandleError [{}] - SO_ERROR = {}", name_, err);
}

void TcpConnection::DefaultConnectionCallback(const TcpConnectionPtr& conn) {
  LOG_TRACE("defaultConnectionCallback {}", conn->LocalAddress().sin_port);
}

void TcpConnection::DefaultMessageCallback(const TcpConnectionPtr& conn,
                                            RingBuffer* buf,
                                            Timestamp receiveTime) {
  buf->RetrieveAll();
}

const char* TcpConnection::StateToString() const {
  switch (state_.load()) {
    case kDisconnected:
      return "kDisconnected";
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    default:
      return "unknown state";
  }
}

}
