/**
 **************************************************************************
 * @file    :Poller.cpp
 * @author  :viviwu
 * @brief   :epoll实现
 * @version :0.1
 * @date    :5/12/26 PM3:38
 * **************************************************************************
 */

#include "Poller.h"
#include "Channel.h"
#include "EventLoop.h"
#include "../base/Logger.h"
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

namespace gateway {

#ifdef __linux__

Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
  if (epollfd_ < 0) {
    LOG_FATAL("Poller::Poller epoll_create1");
  }
}

Poller::~Poller() {
  ::close(epollfd_);
}

Timestamp Poller::Poll(int timeoutMs, ChannelList* activeChannels) {
  int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                               static_cast<int>(events_.size()), timeoutMs);
  int savedErrno = errno;
  Timestamp now(Timestamp::Now());

  if (numEvents > 0) {
    LOG_TRACE("{} events happened", numEvents);
    FillActiveChannels(numEvents, activeChannels);
    if (static_cast<size_t>(numEvents) == events_.size()) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
    LOG_TRACE("nothing happened");
  } else {
    if (savedErrno != EINTR) {
      errno = savedErrno;
      LOG_ERROR("Poller::Poll()");
    }
  }
  return now;
}

void Poller::FillActiveChannels(int numEvents, ChannelList* activeChannels) const {
  (void)numEvents;
  assert(static_cast<size_t>(numEvents) <= events_.size());
  for (int i = 0; i < numEvents; ++i) {
    Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
    channel->SetRevents(events_[i].events);
    activeChannels->push_back(channel);
  }
}

void Poller::UpdateChannel(Channel* channel) {
  ownerLoop_->AssertInLoopThread();
  LOG_TRACE("fd = {} events = {}", channel->Fd(), channel->EventsToString());
  const int index = channel->Index();
  if (index == -1 || index == 2) {
    int fd = channel->Fd();
    if (index == -1) {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;
    } else {
      assert(channels_.find(fd) != channels_.end());
      assert(channels_[fd] == channel);
    }
    channel->SetIndex(1);
    Update(EPOLL_CTL_ADD, channel);
  } else {
    int fd = channel->Fd();
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(index == 1);
    if (channel->IsNoneEvent()) {
      Update(EPOLL_CTL_DEL, channel);
      channel->SetIndex(2);
    } else {
      Update(EPOLL_CTL_MOD, channel);
    }
  }
}

void Poller::RemoveChannel(Channel* channel) {
  ownerLoop_->AssertInLoopThread();
  int fd = channel->Fd();
  LOG_TRACE("Poller::RemoveChannel fd = {}", fd);
  assert(channels_.find(fd) != channels_.end());
  assert(channels_[fd] == channel);
  assert(channel->IsNoneEvent());
  int index = channel->Index();
  assert(index == 1 || index == 2);
  size_t n = channels_.erase(fd);
  assert(n == 1);
  (void)n;

  if (index == 1) {
    Update(EPOLL_CTL_DEL, channel);
  }
  channel->SetIndex(-1);
}

bool Poller::HasChannel(Channel* channel) const {
  ownerLoop_->AssertInLoopThread();
  auto it = channels_.find(channel->Fd());
  return it != channels_.end() && it->second == channel;
}

void Poller::Update(int operation, Channel* channel) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = channel->Events() | EPOLLET;
  event.data.ptr = channel;
  int fd = channel->Fd();
  LOG_TRACE("epoll_ctl op = {} fd = {} event = {{ {} }}",
            operation == EPOLL_CTL_ADD ? "ADD" :
            operation == EPOLL_CTL_DEL ? "DEL" : "MOD",
            fd, channel->EventsToString());
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      LOG_ERROR("epoll_ctl op = DEL fd = {}", fd);
    } else {
      LOG_FATAL("epoll_ctl op = {} fd = {}", operation, fd);
    }
  }
}

#else

// macOS dummy implementation for syntax checking
Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop),
      epollfd_(-1),
      events_(kInitEventListSize) {}

Poller::~Poller() {}

Timestamp Poller::Poll(int timeoutMs, ChannelList* activeChannels) {
  (void)timeoutMs;
  (void)activeChannels;
  return Timestamp::Now();
}

void Poller::FillActiveChannels(int numEvents, ChannelList* activeChannels) const {
  (void)numEvents;
  (void)activeChannels;
}

void Poller::UpdateChannel(Channel* channel) {
  (void)channel;
}

void Poller::RemoveChannel(Channel* channel) {
  (void)channel;
}

bool Poller::HasChannel(Channel* channel) const {
  (void)channel;
  return false;
}

void Poller::Update(int operation, Channel* channel) {
  (void)operation;
  (void)channel;
}

#endif

Poller* Poller::NewDefaultPoller(EventLoop* loop) {
  return new Poller(loop);
}

}
