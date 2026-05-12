/**
 **************************************************************************
 * @file    :Buffer.cpp
 * @author  :viviwu
 * @brief   :Buffer实现（RingBuffer的fd读取）
 * @version :0.1
 * @date    :5/12/26 PM3:39
 * **************************************************************************
 */

#include "Buffer.h"
#include "../base/RingBuffer.h"
#include <errno.h>
#include <sys/uio.h>

namespace gateway {

ssize_t RingBuffer::ReadFd(int fd, int* savedErrno) {
  char extrabuf[65536];
  struct iovec vec[2];
  const size_t writable = WritableBytes();
  vec[0].iov_base = BeginWrite();
  vec[0].iov_len = writable;
  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof(extrabuf);

  const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
  const ssize_t n = ::readv(fd, vec, iovcnt);
  if (n < 0) {
    *savedErrno = errno;
  } else if (static_cast<size_t>(n) <= writable) {
    writeIndex_ += n;
  } else {
    writeIndex_ = buffer_.size();
    Append(extrabuf, n - writable);
  }
  return n;
}

}
