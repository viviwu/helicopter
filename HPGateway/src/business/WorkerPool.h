/**
 **************************************************************************
 * @file    :WorkerPool.h
 * @author  :viviwu
 * @brief   :业务线程池
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#ifndef HELICOPTER_WORKERPOOL_H
#define HELICOPTER_WORKERPOOL_H

#include <functional>
#include <memory>
#include "../base/NonCopyable.h"

namespace gateway {

class ThreadPool;

class WorkerPool : NonCopyable {
 public:
  typedef std::function<void()> Task;

  WorkerPool();
  ~WorkerPool();

  void SetThreadNum(int numThreads);
  void Start();
  void Stop();
  void Submit(Task task);
  size_t QueueSize() const;

 private:
  std::unique_ptr<ThreadPool> pool_;
};

}

#endif  // HELICOPTER_WORKERPOOL_H
