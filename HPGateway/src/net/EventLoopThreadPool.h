/**
 **************************************************************************
 * @file    :EventLoopThreadPool.h
 * @author  :viviwu
 * @brief   :IO线程池（one loop per thread）
 * @version :0.1
 * @date    :5/12/26 PM3:38
 * **************************************************************************
 */

#ifndef HELICOPTER_EVENTLOOPTHREADPOOL_H
#define HELICOPTER_EVENTLOOPTHREADPOOL_H

#include <vector>
#include <memory>
#include <functional>
#include "../base/NonCopyable.h"

namespace gateway {

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : NonCopyable {
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThreadPool(EventLoop* baseLoop, const std::string& name);
  ~EventLoopThreadPool();

  void SetThreadNum(int numThreads) { numThreads_ = numThreads; }
  void SetThreadInitCallback(const ThreadInitCallback& cb) {
    threadInitCallback_ = cb;
  }

  void Start();

  EventLoop* GetNextLoop();
  EventLoop* GetLoopForHash(size_t hashCode);
  std::vector<EventLoop*> GetAllLoops();

  bool Started() const { return started_; }
  const std::string& Name() const { return name_; }

 private:
  EventLoop* baseLoop_;
  std::string name_;
  bool started_;
  int numThreads_;
  int next_;
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop*> loops_;
  ThreadInitCallback threadInitCallback_;
};

}

#endif  // HELICOPTER_EVENTLOOPTHREADPOOL_H
