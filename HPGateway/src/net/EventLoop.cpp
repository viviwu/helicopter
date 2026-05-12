/**
 **************************************************************************
 * @file    :EventLoop.cpp
 * @author  :viviwu
 * @brief   :EventLoop实现
 * @version :0.1
 * @date    :5/12/26 PM3:37
 * **************************************************************************
 */

#include "EventLoop.h"
#include "Channel.h"
#include "Poller.h"
#include "../base/Logger.h"
#include <assert.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <algorithm>

#ifdef __linux__
#include <sys/eventfd.h>
#else
#include <sys/types.h>
#endif

namespace gateway {

__thread EventLoop* t_loopInThisThread = nullptr;

namespace CurrentThread {
  __thread int t_cachedTid = 0;
  __thread char t_tidString[32];
  __thread int t_tidStringLength = 6;
  __thread const char* t_threadName = "unknown";

  void CacheTid() {
    if (t_cachedTid == 0) {
      t_cachedTid = static_cast<int>(::syscall(SYS_gettid));
      t_tidStringLength = snprintf(t_tidString, sizeof(t_tidString), "%5d ", t_cachedTid);
    }
  }
}

const int kPollTimeMs = 10000;

#ifdef __linux__
int CreateEventfd() {
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    LOG_FATAL("Failed in eventfd");
  }
  return evtfd;
}
#else
int CreateEventfd() {
  int fds[2];
  if (::pipe(fds) < 0) {
    LOG_FATAL("Failed in pipe");
  }
  int flags = ::fcntl(fds[0], F_GETFL, 0);
  ::fcntl(fds[0], F_SETFL, flags | O_NONBLOCK);
  flags = ::fcntl(fds[1], F_GETFL, 0);
  ::fcntl(fds[1], F_SETFL, flags | O_NONBLOCK);
  ::close(fds[1]);
  return fds[0];
}
#endif

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      iteration_(0),
      threadId_(CurrentThread::tid()),
      poller_(Poller::NewDefaultPoller(this)),
      wakeupFd_(CreateEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      currentActiveChannel_(nullptr) {
  LOG_DEBUG("EventLoop created {} in thread {}", static_cast<const void*>(this), threadId_);
  if (t_loopInThisThread) {
    LOG_FATAL("Another EventLoop {} exists in this thread {}", static_cast<const void*>(t_loopInThisThread), threadId_);
  } else {
    t_loopInThisThread = this;
  }
  wakeupChannel_->SetReadCallback(std::bind(&EventLoop::HandleRead, this));
  wakeupChannel_->EnableReading();
}

EventLoop::~EventLoop() {
  LOG_DEBUG("EventLoop {} of thread {} destructs in thread {}",
            static_cast<const void*>(this), threadId_, CurrentThread::tid());
  wakeupChannel_->DisableAll();
  wakeupChannel_->Remove();
  ::close(wakeupFd_);
  t_loopInThisThread = nullptr;
}

void EventLoop::Loop() {
  assert(!looping_);
  AssertInLoopThread();
  looping_ = true;
  quit_ = false;

  while (!quit_.load()) {
    activeChannels_.clear();
    pollReturnTime_ = poller_->Poll(kPollTimeMs, &activeChannels_);
    ++iteration_;
    if (Logger::Instance().GetLogger()->should_log(spdlog::level::trace)) {
      PrintActiveChannels();
    }
    eventHandling_ = true;
    for (Channel* channel : activeChannels_) {
      currentActiveChannel_ = channel;
      currentActiveChannel_->HandleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = nullptr;
    eventHandling_ = false;
    DoPendingFunctors();
  }

  LOG_INFO("EventLoop {} stop looping", static_cast<const void*>(this));
  looping_ = false;
}

void EventLoop::Quit() {
  quit_ = true;
  if (!IsInLoopThread()) {
    Wakeup();
  }
}

void EventLoop::RunInLoop(Functor cb) {
  if (IsInLoopThread()) {
    cb();
  } else {
    QueueInLoop(std::move(cb));
  }
}

void EventLoop::QueueInLoop(Functor cb) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    pendingFunctors_.push_back(std::move(cb));
  }

  if (!IsInLoopThread() || callingPendingFunctors_) {
    Wakeup();
  }
}

void EventLoop::Wakeup() {
  uint64_t one = 1;
  ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    LOG_ERROR("EventLoop::Wakeup() writes {} bytes instead of 8", n);
  }
}

void EventLoop::HandleRead() {
  uint64_t one = 1;
  ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    LOG_ERROR("EventLoop::HandleRead() reads {} bytes instead of 8", n);
  }
}

void EventLoop::DoPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
    std::unique_lock<std::mutex> lock(mutex_);
    functors.swap(pendingFunctors_);
  }

  for (const Functor& functor : functors) {
    functor();
  }
  callingPendingFunctors_ = false;
}

void EventLoop::UpdateChannel(Channel* channel) {
  assert(channel->OwnerLoop() == this);
  AssertInLoopThread();
  poller_->UpdateChannel(channel);
}

void EventLoop::RemoveChannel(Channel* channel) {
  assert(channel->OwnerLoop() == this);
  AssertInLoopThread();
  if (eventHandling_) {
    assert(currentActiveChannel_ == channel ||
           std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->RemoveChannel(channel);
}

bool EventLoop::HasChannel(Channel* channel) const {
  assert(channel->OwnerLoop() == this);
  AssertInLoopThread();
  return poller_->HasChannel(channel);
}

void EventLoop::AbortNotInLoopThread() const {
  LOG_FATAL("EventLoop::AbortNotInLoopThread - EventLoop {} was created in threadId_ = {}, current thread id = {}",
            static_cast<const void*>(this), threadId_, CurrentThread::tid());
}

void EventLoop::AssertInLoopThread() const {
  if (!IsInLoopThread()) {
    AbortNotInLoopThread();
  }
}

EventLoop* EventLoop::GetEventLoopOfCurrentThread() {
  return t_loopInThisThread;
}

void EventLoop::PrintActiveChannels() const {
  for (const Channel* channel : activeChannels_) {
    LOG_TRACE("{}", channel->ReventsToString());
  }
}

}
