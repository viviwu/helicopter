/**
 **************************************************************************
 * @file    :Codec.h
 * @author  :viviwu
 * @brief   :协议编解码器
 * @version :0.1
 * @date    :5/12/26 PM3:41
 * **************************************************************************
 */

#ifndef HELICOPTER_CODEC_H
#define HELICOPTER_CODEC_H

#include <functional>
#include <memory>
#include "Packet.h"
#include "../base/RingBuffer.h"
#include "../base/Timestamp.h"

namespace gateway {

class Codec {
 public:
  typedef std::function<void(const Packet&, int64_t connId)> MessageCallback;

  explicit Codec(const MessageCallback& cb) : messageCallback_(cb) {}

  void OnMessage(const std::shared_ptr<class TcpConnection>& conn,
                 RingBuffer* buf,
                 Timestamp receiveTime);

  static void SendPacket(const std::shared_ptr<class TcpConnection>& conn,
                         const Packet& packet);

  static Packet Encode(uint16_t cmd, uint32_t seq, const void* data, size_t len);

 private:
  MessageCallback messageCallback_;
};

}

#endif  // HELICOPTER_CODEC_H
