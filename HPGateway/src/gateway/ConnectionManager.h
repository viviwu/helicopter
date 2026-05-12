/**
 **************************************************************************
 * @file    :ConnectionManager.h
 * @author  :viviwu
 * @brief   :连接管理器（connId映射TcpConnection）
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#ifndef HELICOPTER_CONNECTIONMANAGER_H
#define HELICOPTER_CONNECTIONMANAGER_H

#include <map>
#include <mutex>
#include <memory>
#include <atomic>
#include "../base/NonCopyable.h"

namespace gateway {

class TcpConnection;

class ConnectionManager : NonCopyable {
 public:
  static ConnectionManager& Instance();

  int64_t AddConnection(const std::shared_ptr<TcpConnection>& conn);
  void RemoveConnection(int64_t connId);
  std::shared_ptr<TcpConnection> GetConnection(int64_t connId);

  bool SendToClient(int64_t connId, const void* data, uint32_t len);
  bool Broadcast(const void* data, uint32_t len);

  size_t ConnectionCount() const;

 private:
  ConnectionManager() = default;
  mutable std::mutex mutex_;
  std::map<int64_t, std::weak_ptr<TcpConnection>> connections_;
  std::atomic<int64_t> nextConnId_{1};
};

}

#endif  // HELICOPTER_CONNECTIONMANAGER_H
