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
#include <sstream>
#include <cstdlib>
#include <algorithm>

namespace gateway {

GatewayContext& GatewayContext::Instance() {
  static GatewayContext instance;
  return instance;
}

std::string GatewayContext::Trim(const std::string& s) {
  auto start = std::find_if_not(s.begin(), s.end(), [](char c) {
    return c == ' ' || c == '\t';
  });
  auto end = std::find_if_not(s.rbegin(), s.rend(), [](char c) {
    return c == ' ' || c == '\t' || c == '\r';
  }).base();
  return (start < end) ? std::string(start, end) : std::string();
}

void GatewayContext::ParseLine(const std::string& line) {
  std::string trimmed = Trim(line);

  // 跳过空行和注释
  if (trimmed.empty() || trimmed[0] == '#') return;

  // 跳过 section headers (如 "gateway:")
  if (trimmed.back() == ':' && trimmed.find(' ') == std::string::npos) return;

  size_t colon = trimmed.find(':');
  if (colon == std::string::npos) return;

  std::string key = Trim(trimmed.substr(0, colon));
  std::string value = Trim(trimmed.substr(colon + 1));

  // 去掉引号
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    value = value.substr(1, value.size() - 2);
  }

  if (key == "trade_bind_ip") {
    config_.tradeBindIp = value;
  } else if (key == "trade_port") {
    config_.tradePort = static_cast<uint16_t>(std::atoi(value.c_str()));
  } else if (key == "quote_bind_ip") {
    config_.quoteBindIp = value;
  } else if (key == "quote_port") {
    config_.quotePort = static_cast<uint16_t>(std::atoi(value.c_str()));
  } else if (key == "io_thread_num") {
    config_.ioThreadNum = std::atoi(value.c_str());
  } else if (key == "worker_thread_num") {
    config_.workerThreadNum = std::atoi(value.c_str());
  } else if (key == "max_connections") {
    config_.maxConnections = std::atoi(value.c_str());
  } else if (key == "heartbeat_timeout") {
    config_.heartbeatTimeout = std::atoi(value.c_str());
  } else if (key == "log_level") {
    config_.logLevel = value;
  }
}

bool GatewayContext::LoadConfig(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_WARN("GatewayContext::LoadConfig cannot open {}, using defaults", path);
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    ParseLine(line);
  }

  LOG_INFO("GatewayContext::LoadConfig OK from {} - trade={}:{}, quote={}:{}, "
           "io={}, worker={}, maxConn={}, heartbeat={}s",
           path, config_.tradeBindIp, config_.tradePort,
           config_.quoteBindIp, config_.quotePort,
           config_.ioThreadNum, config_.workerThreadNum,
           config_.maxConnections, config_.heartbeatTimeout);
  return true;
}

}
