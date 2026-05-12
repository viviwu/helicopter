/**
 **************************************************************************
 * @file    :MessageRouter.cpp
 * @author  :viviwu
 * @brief   :消息路由器实现
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#include "MessageRouter.h"
#include "../protocol/Packet.h"
#include "../base/Logger.h"

namespace gateway {

void MessageRouter::RegisterRoute(uint16_t cmd, const RouteHandler& handler) {
  routes_[cmd] = handler;
}

void MessageRouter::Route(const Packet& packet, int64_t connId) {
  auto it = routes_.find(packet.Cmd());
  if (it != routes_.end()) {
    it->second(packet, connId);
  } else {
    LOG_WARN("MessageRouter::Route no route for cmd=0x{:04X}", packet.Cmd());
  }
}

}
