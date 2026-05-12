/**
 **************************************************************************
 * @file    :Thread.cpp
 * @author  :viviwu
 * @brief   :线程实现
 * @version :0.1
 * @date    :5/12/26 PM3:40
 * **************************************************************************
 */

#include "Thread.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <cassert>

namespace gateway {

std::atomic<int> Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false),
      joined_(false),
      tid_(0),
      func_(std::move(func)),
      name_(name) {
  SetDefaultName();
}

Thread::~Thread() {
  if (started_ && !joined_) {
    thread_.detach();
  }
}

void Thread::Start() {
  assert(!started_);
  started_ = true;
  thread_ = std::thread([this]() {
    tid_ = static_cast<pid_t>(::syscall(SYS_gettid));
    func_();
  });
}

void Thread::Join() {
  assert(started_);
  assert(!joined_);
  joined_ = true;
  thread_.join();
}

void Thread::SetDefaultName() {
  if (name_.empty()) {
    int num = ++numCreated_;
    name_ = "Thread" + std::to_string(num);
  }
}

}
