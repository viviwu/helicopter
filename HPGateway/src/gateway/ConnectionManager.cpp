/**
 **************************************************************************
 * @file    :ConnectionManager.cpp
 * @author  :viviwu
 * @brief   :连接管理器实现
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#include "ConnectionManager.h"
#include "../net/TcpConnection.h"
#include "../base/Logger.h"
#include <cassert>

namespace gateway {

ConnectionManager& ConnectionManager::Instance() {
  static ConnectionManager instance;
  return instance;
}

int64_t ConnectionManager::AddConnection(const std::shared_ptr<TcpConnection>& conn) {
  std::unique_lock<std::mutex> lock(mutex_);
  int64_t id = nextConnId_.fetch_add(1);
  connections_[id] = conn;
  return id;
}

void ConnectionManager::RemoveConnection(int64_t connId) {
  std::unique_lock<std::mutex> lock(mutex_);
  connections_.erase(connId);
}

std::shared_ptr<TcpConnection> ConnectionManager::GetConnection(int64_t connId) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = connections_.find(connId);
  if (it != connections_.end()) {
    return it->second.lock();
  }
  return nullptr;
}

bool ConnectionManager::SendToClient(int64_t connId, const void* data, uint32_t len) {
  auto conn = GetConnection(connId);
  if (conn) {
    conn->Send(data, static_cast<int>(len));
    return true;
  }
  return false;
}

bool ConnectionManager::Broadcast(const void* data, uint32_t len) {
  std::unique_lock<std::mutex> lock(mutex_);
  bool allOk = true;
  for (auto& item : connections_) {
    auto conn = item.second.lock();
    if (conn) {
      conn->Send(data, static_cast<int>(len));
    }
  }
  return allOk;
}

size_t ConnectionManager::ConnectionCount() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return connections_.size();
}

}
