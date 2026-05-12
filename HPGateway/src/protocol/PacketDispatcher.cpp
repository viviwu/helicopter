/**
 **************************************************************************
 * @file    :PacketDispatcher.cpp
 * @author  :viviwu
 * @brief   :包分发器实现
 * @version :0.1
 * @date    :5/12/26 PM3:41
 * **************************************************************************
 */

#include "PacketDispatcher.h"
#include "../base/Logger.h"

namespace gateway {

void PacketDispatcher::RegisterHandler(uint16_t cmd, const PacketHandler& handler) {
  handlers_[cmd] = handler;
}

void PacketDispatcher::Dispatch(const Packet& packet, int64_t connId) {
  auto it = handlers_.find(packet.Cmd());
  if (it != handlers_.end()) {
    it->second(packet, connId);
  } else {
    LOG_WARN("PacketDispatcher::Dispatch no handler for cmd=0x{:04X}", packet.Cmd());
  }
}

}
