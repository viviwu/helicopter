/**
 **************************************************************************
 * @file    :Acceptor.h
 * @author  :viviwu
 * @brief   :监听accept
 * @version :0.1
 * @date    :5/12/26 PM3:37
 * **************************************************************************
 */

#ifndef HELICOPTER_ACCEPTOR_H
#define HELICOPTER_ACCEPTOR_H

#include <functional>
#include <memory>
#include <netinet/in.h>
#include "../base/NonCopyable.h"

namespace gateway {

class EventLoop;
class Channel;
class Socket;

class Acceptor : NonCopyable {
 public:
  typedef std::function<void(int sockfd, const struct sockaddr_in&)> NewConnectionCallback;

  Acceptor(EventLoop* loop, const struct sockaddr_in& listenAddr, bool reuseport);
  ~Acceptor();

  void SetNewConnectionCallback(const NewConnectionCallback& cb) {
    newConnectionCallback_ = cb;
  }

  void Listen();
  bool Listening() const { return listening_; }

 private:
  void HandleRead();

  EventLoop* loop_;
  int acceptSocket_;
  std::unique_ptr<Channel> acceptChannel_;
  NewConnectionCallback newConnectionCallback_;
  bool listening_;
  int idleFd_;
};

}

#endif  // HELICOPTER_ACCEPTOR_H
