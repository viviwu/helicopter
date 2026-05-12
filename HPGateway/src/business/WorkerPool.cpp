/**
 **************************************************************************
 * @file    :WorkerPool.cpp
 * @author  :viviwu
 * @brief   :业务线程池实现
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#include "WorkerPool.h"
#include "../base/ThreadPool.h"

namespace gateway {

WorkerPool::WorkerPool()
    : pool_(new ThreadPool("WorkerPool")) {}

WorkerPool::~WorkerPool() {}

void WorkerPool::SetThreadNum(int numThreads) {
  pool_->SetThreadNum(numThreads);
}

void WorkerPool::Start() {
  pool_->Start();
}

void WorkerPool::Stop() {
  pool_->Stop();
}

void WorkerPool::Submit(Task task) {
  pool_->Run(std::move(task));
}

size_t WorkerPool::QueueSize() const {
  return pool_->QueueSize();
}

}
