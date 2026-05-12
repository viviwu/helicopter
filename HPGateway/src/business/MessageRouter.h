/**
 **************************************************************************
 * @file    :MessageRouter.h
 * @author  :viviwu
 * @brief   :消息路由器
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#ifndef HELICOPTER_MESSAGEROUTER_H
#define HELICOPTER_MESSAGEROUTER_H

#include <functional>
#include <map>
#include <cstdint>

namespace gateway {

struct Packet;

class MessageRouter {
 public:
  typedef std::function<void(const Packet& packet, int64_t connId)> RouteHandler;

  void RegisterRoute(uint16_t cmd, const RouteHandler& handler);
  void Route(const Packet& packet, int64_t connId);

 private:
  std::map<uint16_t, RouteHandler> routes_;
};

}

#endif  // HELICOPTER_MESSAGEROUTER_H
