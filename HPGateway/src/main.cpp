/**
 **************************************************************************
 * @file    :main.cpp
 * @author  :viviwu
 * @brief   :Gateway入口
 * @version :0.1
 * @date    :5/12/26 PM3:44
 * **************************************************************************
 */

#include <signal.h>
#include <iostream>
#include "net/EventLoop.h"
#include "net/SocketUtil.h"
#include "gateway/GatewayServer.h"
#include "gateway/GatewayContext.h"
#include "gateway/GatewayApiImpl.h"
#include "base/Logger.h"

using namespace gateway;

namespace {

volatile sig_atomic_t g_running = 1;

void SignalHandler(int sig) {
  LOG_INFO("Receive signal {}, exiting...", sig);
  g_running = 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  // 先用默认级别初始化日志（以便 LoadConfig 能输出日志）
  Logger::Instance().Init("gateway", "info");

  // 加载配置
  GatewayContext::Instance().LoadConfig("conf/gateway.yaml");
  const GatewayConfig& cfg = GatewayContext::Instance().Config();

  // 按配置级别重新设置日志
  if (cfg.logLevel != "info") {
    Logger::Instance().Init("gateway", cfg.logLevel);
  }

  LOG_INFO("HPGateway starting...");

  struct sockaddr_in listenAddr;
  sockets::FromIpPort(cfg.tradeBindIp.c_str(), cfg.tradePort, &listenAddr);

  EventLoop loop;
  GatewayServer server(&loop, listenAddr);
  server.SetThreadNum(cfg.ioThreadNum);
  server.SetWorkerThreadNum(cfg.workerThreadNum);

  GatewayApiImpl api;
  api.Init();

  server.Start();
  LOG_INFO("Gateway listening on {}:{}, ioThreads={}, workerThreads={}",
           cfg.tradeBindIp, cfg.tradePort,
           cfg.ioThreadNum, cfg.workerThreadNum);

  ::signal(SIGINT, SignalHandler);
  ::signal(SIGTERM, SignalHandler);

  loop.Loop();

  api.Release();
  LOG_INFO("Gateway stopped.");
  return 0;
}
