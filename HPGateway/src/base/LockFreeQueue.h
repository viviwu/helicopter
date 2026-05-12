/**
 **************************************************************************
 * @file    :LockFreeQueue.h
 * @author  :viviwu
 * @brief   :MPSC无锁队列（基于moodycamel::ConcurrentQueue）
 * @version :0.1
 * @date    :5/12/26 PM3:40
 * **************************************************************************
 */

#ifndef HELICOPTER_LOCKFREEQUEUE_H
#define HELICOPTER_LOCKFREEQUEUE_H

#include <concurrentqueue/concurrentqueue.h>
#include <memory>

namespace gateway {

template<typename T>
class LockFreeQueue {
 public:
  LockFreeQueue() = default;

  bool Enqueue(const T& item) {
    return queue_.enqueue(item);
  }

  bool Enqueue(T&& item) {
    return queue_.enqueue(std::move(item));
  }

  bool Dequeue(T& item) {
    return queue_.try_dequeue(item);
  }

  size_t Size() const {
    return queue_.size_approx();
  }

  bool Empty() const {
    return queue_.size_approx() == 0;
  }

 private:
  moodycamel::ConcurrentQueue<T> queue_;
};

}

#endif  // HELICOPTER_LOCKFREEQUEUE_H
