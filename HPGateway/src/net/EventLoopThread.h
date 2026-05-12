/**
 **************************************************************************
 * @file    :EventLoopThread.h
 * @author  :viviwu
 * @brief   :IO线程（一个线程一个EventLoop）
 * @version :0.1
 * @date    :5/12/26 PM3:38
 * **************************************************************************
 */

#ifndef HELICOPTER_EVENTLOOPTHREAD_H
#define HELICOPTER_EVENTLOOPTHREAD_H

#include <condition_variable>
#include <mutex>
#include <string>
#include <functional>
#include <thread>
#include "../base/NonCopyable.h"

namespace gateway {

class EventLoop;

class EventLoopThread : NonCopyable {
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const std::string& name = std::string());
  ~EventLoopThread();

  EventLoop* StartLoop();

 private:
  void ThreadFunc();

  EventLoop* loop_;
  bool exiting_;
  std::mutex mutex_;
  std::condition_variable cond_;
  ThreadInitCallback callback_;
  std::thread thread_;
  std::string name_;
};

}

#endif  // HELICOPTER_EVENTLOOPTHREAD_H
