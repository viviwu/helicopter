/**
 **************************************************************************
 * @file    :GatewayApi.h
 * @author  :viviwu
 * @brief   :Gateway API基类（CTP风格）
 * @version :0.1
 * @date    :5/12/26 PM3:30
 * **************************************************************************
 */

#ifndef GATEWAYAPI_H
#define GATEWAYAPI_H

#include <cstdint>

namespace gateway {

class GatewaySpi;

class GatewayApi {
 public:
  virtual ~GatewayApi() {}

  virtual void RegisterSpi(GatewaySpi* spi) = 0;

  virtual bool Init() = 0;

  virtual void Join() = 0;

  virtual void Release() = 0;

  virtual bool SendToClient(int64_t connId, const void* data, uint32_t len) = 0;

  virtual bool Broadcast(const void* data, uint32_t len) = 0;
};

}

#endif  // GATEWAYAPI_H
