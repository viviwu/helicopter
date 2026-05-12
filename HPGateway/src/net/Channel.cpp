/**
 **************************************************************************
 * @file    :Channel.cpp
 * @author  :viviwu
 * @brief   :Channel实现
 * @version :0.1
 * @date    :5/12/26 PM3:37
 * **************************************************************************
 */

#include "Channel.h"
#include "EventLoop.h"
#include "../base/Logger.h"
#include <sstream>

namespace gateway {

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      eventHandling_(false),
      addedToLoop_(false) {}

Channel::~Channel() {
  assert(!eventHandling_);
  assert(!addedToLoop_);
  if (loop_->IsInLoopThread()) {
    assert(!loop_->HasChannel(this));
  }
}

void Channel::HandleEvent(Timestamp receiveTime) {
  eventHandling_ = true;
  LOG_TRACE("channel {} HandleEvent {}", fd_, ReventsToString());

  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    if (closeCallback_) closeCallback_();
  }

  if (revents_ & EPOLLERR) {
    if (errorCallback_) errorCallback_();
  }

  if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
    if (readCallback_) readCallback_(receiveTime);
  }

  if (revents_ & EPOLLOUT) {
    if (writeCallback_) writeCallback_();
  }

  eventHandling_ = false;
}

void Channel::Update() {
  addedToLoop_ = true;
  loop_->UpdateChannel(this);
}

void Channel::Remove() {
  assert(IsNoneEvent());
  addedToLoop_ = false;
  loop_->RemoveChannel(this);
}

std::string Channel::EventsToString() const {
  return EventsToString(fd_, events_);
}

std::string Channel::ReventsToString() const {
  return EventsToString(fd_, revents_);
}

std::string Channel::EventsToString(int fd, int ev) {
  std::ostringstream oss;
  oss << fd << ": ";
  if (ev & EPOLLIN)    oss << "IN ";
  if (ev & EPOLLPRI)   oss << "PRI ";
  if (ev & EPOLLOUT)   oss << "OUT ";
  if (ev & EPOLLHUP)   oss << "HUP ";
  if (ev & EPOLLRDHUP) oss << "RDHUP ";
  if (ev & EPOLLERR)   oss << "ERR ";
  return oss.str();
}

}
