/**
 **************************************************************************
 * @file    :PacketDispatcher.h
 * @author  :viviwu
 * @brief   :包分发器
 * @version :0.1
 * @date    :5/12/26 PM3:41
 * **************************************************************************
 */

#ifndef HELICOPTER_PACKETDISPATCHER_H
#define HELICOPTER_PACKETDISPATCHER_H

#include <functional>
#include <map>
#include "Packet.h"

namespace gateway {

class PacketDispatcher {
 public:
  typedef std::function<void(const Packet& packet, int64_t connId)> PacketHandler;

  void RegisterHandler(uint16_t cmd, const PacketHandler& handler);
  void Dispatch(const Packet& packet, int64_t connId);

 private:
  std::map<uint16_t, PacketHandler> handlers_;
};

}

#endif  // HELICOPTER_PACKETDISPATCHER_H
