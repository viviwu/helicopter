/**
 **************************************************************************
 * @file    :EventLoopThreadPool.cpp
 * @author  :viviwu
 * @brief   :IO线程池实现
 * @version :0.1
 * @date    :5/12/26 PM3:38
 * **************************************************************************
 */

#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "EventLoopThread.h"
#include <assert.h>

namespace gateway {

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& name)
    : baseLoop_(baseLoop),
      name_(name),
      started_(false),
      numThreads_(0),
      next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {}

void EventLoopThreadPool::Start() {
  assert(!started_);
  baseLoop_->AssertInLoopThread();

  started_ = true;

  for (int i = 0; i < numThreads_; ++i) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
    EventLoopThread* t = new EventLoopThread(threadInitCallback_, buf);
    threads_.push_back(std::unique_ptr<EventLoopThread>(t));
    loops_.push_back(t->StartLoop());
  }
  if (numThreads_ == 0 && threadInitCallback_) {
    threadInitCallback_(baseLoop_);
  }
}

EventLoop* EventLoopThreadPool::GetNextLoop() {
  baseLoop_->AssertInLoopThread();
  assert(started_);
  EventLoop* loop = baseLoop_;
  if (!loops_.empty()) {
    loop = loops_[next_];
    ++next_;
    if (static_cast<size_t>(next_) >= loops_.size()) {
      next_ = 0;
    }
  }
  return loop;
}

EventLoop* EventLoopThreadPool::GetLoopForHash(size_t hashCode) {
  baseLoop_->AssertInLoopThread();
  EventLoop* loop = baseLoop_;
  if (!loops_.empty()) {
    loop = loops_[hashCode % loops_.size()];
  }
  return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::GetAllLoops() {
  baseLoop_->AssertInLoopThread();
  assert(started_);
  if (loops_.empty()) {
    return std::vector<EventLoop*>(1, baseLoop_);
  } else {
    return loops_;
  }
}

}
