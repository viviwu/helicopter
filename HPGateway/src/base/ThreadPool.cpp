/**
 **************************************************************************
 * @file    :ThreadPool.cpp
 * @author  :viviwu
 * @brief   :线程池实现
 * @version :0.1
 * @date    :5/12/26 PM3:40
 * **************************************************************************
 */

#include "ThreadPool.h"
#include "Logger.h"
#include <cassert>

namespace gateway {

ThreadPool::ThreadPool(const std::string& name)
    : name_(name),
      running_(false),
      numThreads_(0) {}

ThreadPool::~ThreadPool() {
  if (running_) {
    Stop();
  }
}

void ThreadPool::Start() {
  assert(!running_);
  running_ = true;
  threads_.reserve(numThreads_);
  for (int i = 0; i < numThreads_; ++i) {
    threads_.emplace_back(new std::thread(
        std::bind(&ThreadPool::RunInThread, this)));
  }
  if (numThreads_ == 0) {
    LOG_WARN("ThreadPool {} started with 0 threads", name_);
  }
}

void ThreadPool::Stop() {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    running_ = false;
    cond_.notify_all();
  }
  for (auto& t : threads_) {
    t->join();
  }
}

void ThreadPool::Run(Task task) {
  if (threads_.empty()) {
    task();
  } else {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(std::move(task));
    cond_.notify_one();
  }
}

ThreadPool::Task ThreadPool::Take() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (queue_.empty() && running_) {
    cond_.wait(lock);
  }
  Task task;
  if (!queue_.empty()) {
    task = queue_.front();
    queue_.pop();
  }
  return task;
}

void ThreadPool::RunInThread() {
  while (running_) {
    Task task(Take());
    if (task) {
      task();
    }
  }
}

size_t ThreadPool::QueueSize() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return queue_.size();
}

}
