/**
 **************************************************************************
 * @file    :TcpServer.h
 * @author  :viviwu
 * @brief   :TCP服务器
 * @version :0.1
 * @date    :5/12/26 PM3:39
 * **************************************************************************
 */

#ifndef HELICOPTER_TCPSERVER_H
#define HELICOPTER_TCPSERVER_H

#include <map>
#include <string>
#include <memory>
#include <atomic>
#include <netinet/in.h>
#include "../base/NonCopyable.h"
#include "TcpConnection.h"

namespace gateway {

class EventLoop;
class Acceptor;
class EventLoopThreadPool;

class TcpServer : NonCopyable {
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;
  enum Option {
    kNoReusePort,
    kReusePort,
  };

  TcpServer(EventLoop* loop,
            const struct sockaddr_in& listenAddr,
            const std::string& name,
            Option option = kNoReusePort);
  ~TcpServer();

  const std::string& IpPort() const { return ipPort_; }
  const std::string& Name() const { return name_; }
  EventLoop* GetLoop() const { return loop_; }

  void SetThreadNum(int numThreads);
  void SetThreadInitCallback(const ThreadInitCallback& cb) {
    threadInitCallback_ = cb;
  }

  void Start();

  void SetConnectionCallback(const TcpConnection::ConnectionCallback& cb) {
    connectionCallback_ = cb;
  }
  void SetMessageCallback(const TcpConnection::MessageCallback& cb) {
    messageCallback_ = cb;
  }
  void SetWriteCompleteCallback(const TcpConnection::WriteCompleteCallback& cb) {
    writeCompleteCallback_ = cb;
  }

 private:
  void NewConnection(int sockfd, const struct sockaddr_in& peerAddr);
  void RemoveConnection(const TcpConnection::TcpConnectionPtr& conn);
  void RemoveConnectionInLoop(const TcpConnection::TcpConnectionPtr& conn);

  typedef std::map<std::string, TcpConnection::TcpConnectionPtr> ConnectionMap;

  EventLoop* loop_;
  const std::string ipPort_;
  const std::string name_;
  std::unique_ptr<Acceptor> acceptor_;
  std::shared_ptr<EventLoopThreadPool> threadPool_;
  TcpConnection::ConnectionCallback connectionCallback_;
  TcpConnection::MessageCallback messageCallback_;
  TcpConnection::WriteCompleteCallback writeCompleteCallback_;
  ThreadInitCallback threadInitCallback_;
  std::atomic<int> started_;
  int nextConnId_;
  ConnectionMap connections_;
};

}

#endif  // HELICOPTER_TCPSERVER_H
