/**
 **************************************************************************
 * @file    :NonCopyable.h
 * @author  :viviwu
 * @brief   :禁用拷贝基类
 * @version :0.1
 * @date    :5/12/26 PM3:39
 * **************************************************************************
 */

#ifndef HELICOPTER_NONCOPYABLE_H
#define HELICOPTER_NONCOPYABLE_H

namespace gateway {

class NonCopyable {
 public:
  NonCopyable() = default;
  ~NonCopyable() = default;

  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
};

}

#endif  // HELICOPTER_NONCOPYABLE_H
