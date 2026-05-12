/**
 **************************************************************************
 * @file    :SessionManager.cpp
 * @author  :viviwu
 * @brief   :会话管理器实现
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#include "SessionManager.h"
#include "../base/Logger.h"
#include "../base/Timestamp.h"

namespace gateway {

SessionManager& SessionManager::Instance() {
  static SessionManager instance;
  return instance;
}

bool SessionManager::AddSession(int64_t connId) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (sessions_.find(connId) != sessions_.end()) {
    return false;
  }
  Session s;
  s.connId = connId;
  s.loggedIn = false;
  s.loginTime = 0;
  s.lastHeartbeat = Timestamp::Now().MilliSecondsSinceEpoch();
  sessions_[connId] = s;
  return true;
}

bool SessionManager::RemoveSession(int64_t connId) {
  std::unique_lock<std::mutex> lock(mutex_);
  return sessions_.erase(connId) > 0;
}

bool SessionManager::Login(int64_t connId, const std::string& account) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = sessions_.find(connId);
  if (it == sessions_.end()) return false;
  it->second.loggedIn = true;
  it->second.account = account;
  it->second.loginTime = Timestamp::Now().MilliSecondsSinceEpoch();
  return true;
}

bool SessionManager::Logout(int64_t connId) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = sessions_.find(connId);
  if (it == sessions_.end()) return false;
  it->second.loggedIn = false;
  return true;
}

bool SessionManager::IsLoggedIn(int64_t connId) const {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = sessions_.find(connId);
  if (it == sessions_.end()) return false;
  return it->second.loggedIn;
}

void SessionManager::UpdateHeartbeat(int64_t connId) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = sessions_.find(connId);
  if (it != sessions_.end()) {
    it->second.lastHeartbeat = Timestamp::Now().MilliSecondsSinceEpoch();
  }
}

Session* SessionManager::GetSession(int64_t connId) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = sessions_.find(connId);
  if (it != sessions_.end()) {
    return &it->second;
  }
  return nullptr;
}

size_t SessionManager::SessionCount() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return sessions_.size();
}

}
