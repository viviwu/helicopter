/**
 **************************************************************************
 * @file    :Channel.h
 * @author  :viviwu
 * @brief   :IO事件分发通道
 * @version :0.1
 * @date    :5/12/26 PM3:37
 * **************************************************************************
 */

#ifndef HELICOPTER_CHANNEL_H
#define HELICOPTER_CHANNEL_H

#include <functional>
#include <memory>
#include <string>
#include "../base/Timestamp.h"

#ifdef __linux__
#include <sys/epoll.h>
#else
// macOS fallback: define epoll constants for syntax compatibility
#define EPOLLIN 0x001
#define EPOLLPRI 0x002
#define EPOLLOUT 0x004
#define EPOLLERR 0x008
#define EPOLLHUP 0x010
#define EPOLLRDHUP 0x2000
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3
#define EPOLLET (1U << 31)
#endif

namespace gateway {

class EventLoop;

class Channel {
 public:
  typedef std::function<void()> EventCallback;
  typedef std::function<void(Timestamp)> ReadEventCallback;

  Channel(EventLoop* loop, int fd);
  ~Channel();

  void HandleEvent(Timestamp receiveTime);

  void SetReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
  void SetWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void SetCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void SetErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  int Fd() const { return fd_; }
  int Events() const { return events_; }
  void SetRevents(int revt) { revents_ = revt; }
  bool IsNoneEvent() const { return events_ == kNoneEvent; }

  void EnableReading() { events_ |= kReadEvent; Update(); }
  void DisableReading() { events_ &= ~kReadEvent; Update(); }
  void EnableWriting() { events_ |= kWriteEvent; Update(); }
  void DisableWriting() { events_ &= ~kWriteEvent; Update(); }
  void DisableAll() { events_ = kNoneEvent; Update(); }
  bool IsWriting() const { return events_ & kWriteEvent; }
  bool IsReading() const { return events_ & kReadEvent; }

  int Index() const { return index_; }
  void SetIndex(int idx) { index_ = idx; }

  EventLoop* OwnerLoop() { return loop_; }
  void Remove();

  std::string EventsToString() const;
  std::string ReventsToString() const;

 private:
  static std::string EventsToString(int fd, int ev);
  void Update();

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_;
  const int fd_;
  int events_;
  int revents_;
  int index_;

  bool eventHandling_;
  bool addedToLoop_;

  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

}

#endif  // HELICOPTER_CHANNEL_H
