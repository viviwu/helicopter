/**
 **************************************************************************
 * @file    :GatewayContext.h
 * @author  :viviwu
 * @brief   :网关上下文
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#ifndef HELICOPTER_GATEWAYCONTEXT_H
#define HELICOPTER_GATEWAYCONTEXT_H

#include <string>
#include <cstdint>

namespace gateway {

struct GatewayConfig {
  std::string tradeBindIp = "0.0.0.0";
  uint16_t tradePort = 8001;
  std::string quoteBindIp = "0.0.0.0";
  uint16_t quotePort = 8002;
  int ioThreadNum = 4;
  int workerThreadNum = 4;
  int maxConnections = 10000;
};

class GatewayContext {
 public:
  static GatewayContext& Instance();

  bool LoadConfig(const std::string& path);

  const GatewayConfig& Config() const { return config_; }
  GatewayConfig& MutableConfig() { return config_; }

 private:
  GatewayContext() = default;
  GatewayConfig config_;
};

}

#endif  // HELICOPTER_GATEWAYCONTEXT_H
