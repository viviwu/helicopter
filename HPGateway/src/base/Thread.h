/**
 **************************************************************************
 * @file    :Thread.h
 * @author  :viviwu
 * @brief   :线程封装
 * @version :0.1
 * @date    :5/12/26 PM3:40
 * **************************************************************************
 */

#ifndef HELICOPTER_THREAD_H
#define HELICOPTER_THREAD_H

#include <thread>
#include <functional>
#include <string>
#include <atomic>
#include "NonCopyable.h"

namespace gateway {

class Thread : NonCopyable {
 public:
  typedef std::function<void()> ThreadFunc;

  explicit Thread(ThreadFunc func, const std::string& name = std::string());
  ~Thread();

  void Start();
  void Join();

  bool Started() const { return started_; }
  pid_t Tid() const { return tid_; }
  const std::string& Name() const { return name_; }

  static int NumCreated() { return numCreated_.load(); }

 private:
  void SetDefaultName();

  bool started_;
  bool joined_;
  std::thread thread_;
  pid_t tid_;
  ThreadFunc func_;
  std::string name_;
  static std::atomic<int> numCreated_;
};

}

#endif  // HELICOPTER_THREAD_H
