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

void SignalHandler(int sig) {
  LOG_INFO("Receive signal {}, exiting...", sig);
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  Logger::Instance().Init("gateway", "info");
  LOG_INFO("HPGateway starting...");

  GatewayContext::Instance().LoadConfig("conf/gateway.yaml");

  const GatewayConfig& cfg = GatewayContext::Instance().Config();

  struct sockaddr_in listenAddr;
  sockets::FromIpPort(cfg.tradeBindIp.c_str(), cfg.tradePort, &listenAddr);

  EventLoop loop;
  GatewayServer server(&loop, listenAddr);
  server.SetThreadNum(cfg.ioThreadNum);
  server.SetWorkerThreadNum(cfg.workerThreadNum);

  GatewayApiImpl api;
  api.Init();

  server.Start();
  LOG_INFO("Gateway listening on {}:{}", cfg.tradeBindIp, cfg.tradePort);

  loop.Loop();

  api.Release();
  LOG_INFO("Gateway stopped.");
  return 0;
}
