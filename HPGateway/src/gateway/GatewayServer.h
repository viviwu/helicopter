/**
 **************************************************************************
 * @file    :GatewayServer.h
 * @author  :viviwu
 * @brief   :网关服务器（组合TcpServer + 业务线程池 + 心跳）
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#ifndef HELICOPTER_GATEWAYSERVER_H
#define HELICOPTER_GATEWAYSERVER_H

#include <memory>
#include <atomic>
#include <map>
#include <netinet/in.h>
#include "../base/NonCopyable.h"
#include "../base/Timestamp.h"
#include "../protocol/Packet.h"
#include "../protocol/PacketDispatcher.h"

namespace gateway {

class EventLoop;
class TcpServer;
class Channel;
class ThreadPool;
class Codec;
class GatewaySpi;

class GatewayServer : NonCopyable {
 public:
  GatewayServer(EventLoop* loop, const struct sockaddr_in& listenAddr);
  ~GatewayServer();

  void SetThreadNum(int numThreads);
  void SetWorkerThreadNum(int numThreads);
  void SetSpi(GatewaySpi* spi) { spi_ = spi; }

  void Start();
  void Stop();

  PacketDispatcher& GetDispatcher() { return dispatcher_; }

 private:
  void OnConnection(const std::shared_ptr<class TcpConnection>& conn);
  void OnMessage(const std::shared_ptr<class TcpConnection>& conn,
                 class RingBuffer* buf,
                 Timestamp receiveTime);
  void OnWriteComplete(const std::shared_ptr<class TcpConnection>& conn);

  void OnPacket(const Packet& packet, int64_t connId);

  // 心跳
  void SetupHeartbeat();
  void OnHeartbeat(Timestamp receiveTime);

  EventLoop* loop_;
  std::unique_ptr<TcpServer> server_;
  std::unique_ptr<Codec> codec_;
  std::unique_ptr<ThreadPool> workerPool_;
  PacketDispatcher dispatcher_;
  GatewaySpi* spi_;
  std::atomic<bool> started_;

  // 心跳定时器
  int heartbeatTimerFd_;
  std::unique_ptr<Channel> heartbeatChannel_;
  int heartbeatTimeoutSec_;
  std::map<int64_t, int64_t> lastHeartbeats_;
};

}

#endif  // HELICOPTER_GATEWAYSERVER_H
