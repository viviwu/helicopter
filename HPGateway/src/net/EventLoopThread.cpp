/**
 **************************************************************************
 * @file    :EventLoopThread.cpp
 * @author  :viviwu
 * @brief   :IO线程实现
 * @version :0.1
 * @date    :5/12/26 PM3:38
 * **************************************************************************
 */

#include "EventLoopThread.h"
#include "EventLoop.h"
#include "../base/Logger.h"

namespace gateway {

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const std::string& name)
    : loop_(NULL),
      exiting_(false),
      callback_(cb),
      name_(name) {}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  if (loop_ != NULL) {
    loop_->Quit();
    thread_.join();
  }
}

EventLoop* EventLoopThread::StartLoop() {
  assert(!thread_.joinable());
  thread_ = std::thread(std::bind(&EventLoopThread::ThreadFunc, this));

  EventLoop* loop = NULL;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    while (loop_ == NULL) {
      cond_.wait(lock);
    }
    loop = loop_;
  }
  return loop;
}

void EventLoopThread::ThreadFunc() {
  EventLoop loop;

  if (callback_) {
    callback_(&loop);
  }

  {
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = &loop;
    cond_.notify_one();
  }

  loop.Loop();

  std::unique_lock<std::mutex> lock(mutex_);
  loop_ = NULL;
}

}
