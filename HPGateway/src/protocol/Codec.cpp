/**
 **************************************************************************
 * @file    :Codec.cpp
 * @author  :viviwu
 * @brief   :协议编解码实现
 * @version :0.1
 * @date    :5/12/26 PM3:41
 * **************************************************************************
 */

#include "Codec.h"
#include "../net/TcpConnection.h"
#include "../base/Logger.h"
#include <assert.h>
#include <string.h>

namespace gateway {

void Codec::OnMessage(const std::shared_ptr<TcpConnection>& conn,
                      RingBuffer* buf,
                      Timestamp receiveTime) {
  while (buf->ReadableBytes() >= PacketHeader::kHeaderLength) {
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buf->Peek());
    uint32_t len = header->length;
    if (len > 64 * 1024) {
      LOG_ERROR("Codec::OnMessage invalid length {}", len);
      conn->ForceClose();
      break;
    }
    if (buf->ReadableBytes() >= len) {
      Packet packet;
      memcpy(&packet.header, header, PacketHeader::kHeaderLength);
      if (len > PacketHeader::kHeaderLength) {
        packet.body.resize(len - PacketHeader::kHeaderLength);
        memcpy(&packet.body[0], buf->Peek() + PacketHeader::kHeaderLength,
               packet.body.size());
      }
      buf->Retrieve(len);
      if (messageCallback_) {
        messageCallback_(packet, 0);
      }
    } else {
      break;
    }
  }
}

void Codec::SendPacket(const std::shared_ptr<TcpConnection>& conn,
                       const Packet& packet) {
  assert(packet.TotalLength() >= PacketHeader::kHeaderLength);
  RingBuffer buf;
  buf.Append(reinterpret_cast<const char*>(&packet.header), PacketHeader::kHeaderLength);
  if (!packet.body.empty()) {
    buf.Append(&packet.body[0], packet.body.size());
  }
  conn->Send(&buf);
}

Packet Codec::Encode(uint16_t cmd, uint32_t seq, const void* data, size_t len) {
  Packet packet;
  packet.header.length = PacketHeader::kHeaderLength + static_cast<uint32_t>(len);
  packet.header.cmd = cmd;
  packet.header.flag = 0;
  packet.header.seq = seq;
  if (len > 0) {
    packet.body.resize(len);
    memcpy(&packet.body[0], data, len);
  }
  return packet;
}

}
