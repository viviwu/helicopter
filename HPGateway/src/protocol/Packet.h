/**
 **************************************************************************
 * @file    :Packet.h
 * @author  :viviwu
 * @brief   :协议包定义
 * @version :0.1
 * @date    :5/12/26 PM3:41
 * **************************************************************************
 */

#ifndef HELICOPTER_PACKET_H
#define HELICOPTER_PACKET_H

#include <cstdint>
#include <vector>
#include <string>

namespace gateway {

#pragma pack(push, 1)

struct PacketHeader {
  uint32_t length;   // 整个包长度（含头）
  uint16_t cmd;      // 命令号
  uint16_t flag;     // 标志位
  uint32_t seq;      // 序列号

  static const int kHeaderLength = 12;
};

#pragma pack(pop)

struct Packet {
  PacketHeader header;
  std::vector<char> body;

  uint32_t TotalLength() const { return header.length; }
  uint16_t Cmd() const { return header.cmd; }
  bool IsValid() const {
    return header.length >= PacketHeader::kHeaderLength;
  }
};

enum CmdType : uint16_t {
  kCmdLoginReq        = 0x0001,
  kCmdLoginRsp        = 0x0002,
  kCmdLogoutReq       = 0x0003,
  kCmdLogoutRsp       = 0x0004,
  kCmdOrderInsertReq  = 0x0101,
  kCmdOrderInsertRsp  = 0x0102,
  kCmdOrderCancelReq  = 0x0103,
  kCmdOrderCancelRsp  = 0x0104,
  kCmdSubscribeReq    = 0x0201,
  kCmdSubscribeRsp    = 0x0202,
  kCmdUnsubscribeReq  = 0x0203,
  kCmdUnsubscribeRsp  = 0x0204,
  kCmdTickNotify      = 0x0301,
  kCmdOrderNotify     = 0x0302,
  kCmdTradeNotify     = 0x0303,
  kCmdHeartbeatReq    = 0xF001,
  kCmdHeartbeatRsp    = 0xF002,
};

}

#endif  // HELICOPTER_PACKET_H
