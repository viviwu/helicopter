/**
 **************************************************************************
 * @file    :EventLoop.h
 * @author  :viviwu
 * @brief   :事件循环（one loop per thread）
 * @version :0.1
 * @date    :5/12/26 PM3:37
 * **************************************************************************
 */

#ifndef HELICOPTER_EVENTLOOP_H
#define HELICOPTER_EVENTLOOP_H

#include <atomic>
#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include "../base/NonCopyable.h"
#include "../base/Timestamp.h"

namespace gateway {

namespace CurrentThread {
  extern __thread int t_cachedTid;
  extern __thread char t_tidString[32];
  extern __thread int t_tidStringLength;
  extern __thread const char* t_threadName;
  void CacheTid();
  inline int tid() {
    if (__builtin_expect(t_cachedTid == 0, 0)) {
      CacheTid();
    }
    return t_cachedTid;
  }
  inline const char* tidString() {
    return t_tidString;
  }
  inline int tidStringLength() {
    return t_tidStringLength;
  }
  inline const char* name() {
    return t_threadName;
  }
}

class Channel;
class Poller;
class TimerQueue;

class EventLoop : NonCopyable {
 public:
  typedef std::function<void()> Functor;

  EventLoop();
  ~EventLoop();

  void Loop();
  void Quit();

  Timestamp PollReturnTime() const { return pollReturnTime_; }

  void RunInLoop(Functor cb);
  void QueueInLoop(Functor cb);

  void Wakeup();

  void UpdateChannel(Channel* channel);
  void RemoveChannel(Channel* channel);
  bool HasChannel(Channel* channel) const;

  bool IsInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  void AssertInLoopThread() const;
  static EventLoop* GetEventLoopOfCurrentThread();

 private:
  void AbortNotInLoopThread() const;
  void HandleRead();
  void DoPendingFunctors();

  void PrintActiveChannels() const;

  typedef std::vector<Channel*> ChannelList;

  bool looping_;
  std::atomic<bool> quit_;
  bool eventHandling_;
  bool callingPendingFunctors_;
  int64_t iteration_;
  const pid_t threadId_;
  Timestamp pollReturnTime_;
  std::unique_ptr<Poller> poller_;
  int wakeupFd_;
  std::unique_ptr<Channel> wakeupChannel_;
  ChannelList activeChannels_;
  Channel* currentActiveChannel_;

  mutable std::mutex mutex_;
  std::vector<Functor> pendingFunctors_;
};

}

#endif  // HELICOPTER_EVENTLOOP_H
