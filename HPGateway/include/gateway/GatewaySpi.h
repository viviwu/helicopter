/**
 **************************************************************************
 * @file    :GatewaySpi.h
 * @author  :viviwu
 * @brief   :Gateway SPI回调接口（CTP风格）
 * @version :0.1
 * @date    :5/12/26 PM3:55
 * **************************************************************************
 */

#ifndef HELICOPTER_GATEWAYSPI_H
#define HELICOPTER_GATEWAYSPI_H

#include "GatewayStruct.h"

namespace gateway {

struct Packet;

class GatewaySpi {
 public:
  virtual ~GatewaySpi() {}

  virtual void OnConnected() {}

  virtual void OnDisconnected(int reason) {}

  virtual void OnClientLogin(
      const LoginRequest* req,
      int connId) {}

  virtual void OnMessage(
      const Packet* packet,
      int connId) {}

  virtual void OnError(
      int errorCode,
      const char* msg) {}
};

}

#endif  // HELICOPTER_GATEWAYSPI_H
