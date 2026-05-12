/**
 **************************************************************************
 * @file    :RingBuffer.h
 * @author  :viviwu
 * @brief   :环形缓冲区（一写一读，无锁）
 * @version :0.1
 * @date    :5/12/26 PM3:41
 * **************************************************************************
 */

#ifndef HELICOPTER_RINGBUFFER_H
#define HELICOPTER_RINGBUFFER_H

#include <vector>
#include <algorithm>
#include <cassert>
#include <cstring>

namespace gateway {

class RingBuffer {
 public:
  explicit RingBuffer(size_t initialSize = 1024)
      : buffer_(initialSize),
        readIndex_(0),
        writeIndex_(0) {}

  size_t ReadableBytes() const {
    return writeIndex_ - readIndex_;
  }

  size_t WritableBytes() const {
    return buffer_.size() - writeIndex_;
  }

  size_t PrependableBytes() const {
    return readIndex_;
  }

  const char* Peek() const {
    return Begin() + readIndex_;
  }

  void Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    if (len < ReadableBytes()) {
      readIndex_ += len;
    } else {
      RetrieveAll();
    }
  }

  void RetrieveUntil(const char* end) {
    assert(Peek() <= end);
    assert(end <= BeginWrite());
    Retrieve(end - Peek());
  }

  void RetrieveAll() {
    readIndex_ = 0;
    writeIndex_ = 0;
  }

  std::string RetrieveAsString(size_t len) {
    assert(len <= ReadableBytes());
    std::string result(Peek(), len);
    Retrieve(len);
    return result;
  }

  std::string RetrieveAllAsString() {
    return RetrieveAsString(ReadableBytes());
  }

  void Append(const char* data, size_t len) {
    EnsureWritableBytes(len);
    std::copy(data, data + len, BeginWrite());
    HasWritten(len);
  }

  void Append(const void* data, size_t len) {
    Append(static_cast<const char*>(data), len);
  }

  void EnsureWritableBytes(size_t len) {
    if (WritableBytes() < len) {
      MakeSpace(len);
    }
    assert(WritableBytes() >= len);
  }

  char* BeginWrite() {
    return Begin() + writeIndex_;
  }

  const char* BeginWrite() const {
    return Begin() + writeIndex_;
  }

  void HasWritten(size_t len) {
    assert(len <= WritableBytes());
    writeIndex_ += len;
  }

  void Unwrite(size_t len) {
    assert(len <= ReadableBytes());
    writeIndex_ -= len;
  }

  void Prepend(const void* data, size_t len) {
    assert(len <= PrependableBytes());
    readIndex_ -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d, d + len, Begin() + readIndex_);
  }

  void Shrink(size_t reserve) {
    std::vector<char> buf(reserve);
    size_t readable = ReadableBytes();
    std::copy(Peek(), Peek() + readable, buf.begin());
    buf.swap(buffer_);
    readIndex_ = 0;
    writeIndex_ = readable;
  }

  size_t Capacity() const { return buffer_.capacity(); }

  ssize_t ReadFd(int fd, int* savedErrno);

 private:
  char* Begin() {
    return &*buffer_.begin();
  }

  const char* Begin() const {
    return &*buffer_.begin();
  }

  void MakeSpace(size_t len) {
    if (WritableBytes() + PrependableBytes() < len + kCheapPrepend) {
      buffer_.resize(writeIndex_ + len);
    } else {
      size_t readable = ReadableBytes();
      std::copy(Begin() + readIndex_,
                Begin() + writeIndex_,
                Begin() + kCheapPrepend);
      readIndex_ = kCheapPrepend;
      writeIndex_ = readIndex_ + readable;
    }
  }

  static const size_t kCheapPrepend = 8;

  std::vector<char> buffer_;
  size_t readIndex_;
  size_t writeIndex_;
};

}

#endif  // HELICOPTER_RINGBUFFER_H
