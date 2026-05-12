/**
 **************************************************************************
 * @file    :GatewayContext.cpp
 * @author  :viviwu
 * @brief   :网关上下文实现
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#include "GatewayContext.h"
#include "../base/Logger.h"
#include <fstream>

namespace gateway {

GatewayContext& GatewayContext::Instance() {
  static GatewayContext instance;
  return instance;
}

bool GatewayContext::LoadConfig(const std::string& path) {
  // 简化版：暂时不使用yaml解析，后续可接入yaml-cpp
  LOG_INFO("GatewayContext::LoadConfig from {}", path);
  return true;
}

}
