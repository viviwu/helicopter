/**
 **************************************************************************
 * @file    :ThreadPool.h
 * @author  :viviwu
 * @brief   :线程池
 * @version :0.1
 * @date    :5/12/26 PM3:40
 * **************************************************************************
 */

#ifndef HELICOPTER_THREADPOOL_H
#define HELICOPTER_THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <string>
#include "NonCopyable.h"

namespace gateway {

class ThreadPool : NonCopyable {
 public:
  typedef std::function<void()> Task;

  explicit ThreadPool(const std::string& name = std::string("ThreadPool"));
  ~ThreadPool();

  void SetThreadNum(int numThreads) { numThreads_ = numThreads; }

  void Start();
  void Stop();

  void Run(Task task);

  size_t QueueSize() const;

 private:
  void RunInThread();
  Task Take();

  mutable std::mutex mutex_;
  std::condition_variable cond_;
  std::string name_;
  std::vector<std::unique_ptr<std::thread>> threads_;
  std::queue<Task> queue_;
  bool running_;
  int numThreads_;
};

}

#endif  // HELICOPTER_THREADPOOL_H
