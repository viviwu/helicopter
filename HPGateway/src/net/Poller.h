/**
 **************************************************************************
 * @file    :Poller.h
 * @author  :viviwu
 * @brief   :epoll封装
 * @version :0.1
 * @date    :5/12/26 PM3:38
 * **************************************************************************
 */

#ifndef HELICOPTER_POLLER_H
#define HELICOPTER_POLLER_H

#include <map>
#include <vector>
#include "../base/Timestamp.h"

#ifdef __linux__
#include <sys/epoll.h>
#else
struct epoll_event {
  unsigned int events;
  void* data_ptr;
  void* data;
};
#endif

namespace gateway {

class Channel;
class EventLoop;

class Poller {
 public:
  typedef std::vector<Channel*> ChannelList;

  explicit Poller(EventLoop* loop);
  ~Poller();

  Timestamp Poll(int timeoutMs, ChannelList* activeChannels);
  void UpdateChannel(Channel* channel);
  void RemoveChannel(Channel* channel);
  bool HasChannel(Channel* channel) const;

  static Poller* NewDefaultPoller(EventLoop* loop);

 private:
  static const int kInitEventListSize = 16;

  void FillActiveChannels(int numEvents, ChannelList* activeChannels) const;
  void Update(int operation, Channel* channel);

  typedef std::vector<struct epoll_event> EventList;
  typedef std::map<int, Channel*> ChannelMap;

  EventLoop* ownerLoop_;
  int epollfd_;
  EventList events_;
  ChannelMap channels_;
};

}

#endif  // HELICOPTER_POLLER_H
