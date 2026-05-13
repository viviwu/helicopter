/**
 **************************************************************************
 * @file    :Buffer.h
 * @author  :viviwu
 * @brief   :网络层Buffer（基于RingBuffer，ReadFd实现在Buffer.cpp）
 * @version :0.1
 * @date    :5/12/26 PM3:39
 * **************************************************************************
 */

#ifndef HELICOPTER_BUFFER_H
#define HELICOPTER_BUFFER_H

#include "../base/RingBuffer.h"

// Buffer.h 是网络层对 RingBuffer 的再导出
// RingBuffer::ReadFd 的实现在 Buffer.cpp 中，放在 net 层以保持网络 I/O 的归属清晰

#endif  // HELICOPTER_BUFFER_H
