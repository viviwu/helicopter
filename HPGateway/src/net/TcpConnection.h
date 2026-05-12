/**
 **************************************************************************
 * @file    :TcpConnection.h
 * @author  :viviwu
 * @brief   :TCP连接
 * @version :0.1
 * @date    :5/12/26 PM3:38
 * **************************************************************************
 */

#ifndef HELICOPTER_TCPCONNECTION_H
#define HELICOPTER_TCPCONNECTION_H

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <netinet/in.h>
#include "../base/NonCopyable.h"
#include "../base/Timestamp.h"
#include "../base/RingBuffer.h"

namespace gateway {

class EventLoop;
class Channel;

class TcpConnection : NonCopyable,
                      public std::enable_shared_from_this<TcpConnection> {
 public:
  enum State { kDisconnected, kConnecting, kConnected, kDisconnecting };

  typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
  typedef std::function<void(const TcpConnectionPtr&)> ConnectionCallback;
  typedef std::function<void(const TcpConnectionPtr&,
                             RingBuffer*, Timestamp)> MessageCallback;
  typedef std::function<void(const TcpConnectionPtr&)> WriteCompleteCallback;
  typedef std::function<void(const TcpConnectionPtr&)> CloseCallback;
  typedef std::function<void(const TcpConnectionPtr&, size_t)> HighWaterMarkCallback;

  TcpConnection(EventLoop* loop,
                const std::string& name,
                int sockfd,
                const struct sockaddr_in& localAddr,
                const struct sockaddr_in& peerAddr);
  ~TcpConnection();

  EventLoop* GetLoop() const { return loop_; }
  const std::string& Name() const { return name_; }
  const struct sockaddr_in& LocalAddress() const { return localAddr_; }
  const struct sockaddr_in& PeerAddress() const { return peerAddr_; }
  bool Connected() const { return state_ == kConnected; }
  bool Disconnected() const { return state_ == kDisconnected; }

  void SetConnId(int64_t connId) { connId_ = connId; }
  int64_t GetConnId() const { return connId_; }

  void Send(const void* message, int len);
  void Send(const std::string& message);
  void Send(RingBuffer* message);
  void Shutdown();
  void ForceClose();

  void SetTcpNoDelay(bool on);

  void SetConnectionCallback(const ConnectionCallback& cb) {
    connectionCallback_ = cb;
  }
  void SetMessageCallback(const MessageCallback& cb) {
    messageCallback_ = cb;
  }
  void SetWriteCompleteCallback(const WriteCompleteCallback& cb) {
    writeCompleteCallback_ = cb;
  }
  void SetHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) {
    highWaterMarkCallback_ = cb;
    highWaterMark_ = highWaterMark;
  }
  void SetCloseCallback(const CloseCallback& cb) {
    closeCallback_ = cb;
  }

  RingBuffer* InputBuffer() { return &inputBuffer_; }
  RingBuffer* OutputBuffer() { return &outputBuffer_; }

  static void DefaultConnectionCallback(const TcpConnectionPtr& conn);
  static void DefaultMessageCallback(const TcpConnectionPtr& conn,
                                     RingBuffer* buf,
                                     Timestamp receiveTime);

  void ConnectEstablished();
  void ConnectDestroyed();

 private:
  void SetState(State s) { state_.store(s); }
  const char* StateToString() const;

  void HandleRead(Timestamp receiveTime);
  void HandleWrite();
  void HandleClose();
  void HandleError();

  void SendInLoop(const std::string& message);
  void SendInLoop(const void* message, size_t len);
  void ShutdownInLoop();
  void ForceCloseInLoop();

  EventLoop* loop_;
  const std::string name_;
  std::atomic<State> state_;
  bool reading_;

  int socket_;
  std::unique_ptr<Channel> channel_;

  const struct sockaddr_in localAddr_;
  const struct sockaddr_in peerAddr_;

  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  HighWaterMarkCallback highWaterMarkCallback_;
  CloseCallback closeCallback_;
  size_t highWaterMark_;
  int64_t connId_;

  RingBuffer inputBuffer_;
  RingBuffer outputBuffer_;
};

}

#endif  // HELICOPTER_TCPCONNECTION_H
