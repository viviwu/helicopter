/**
 **************************************************************************
 * @file    :SessionManager.h
 * @author  :viviwu
 * @brief   :会话管理器
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#ifndef HELICOPTER_SESSIONMANAGER_H
#define HELICOPTER_SESSIONMANAGER_H

#include <string>
#include <map>
#include <mutex>
#include <cstdint>
#include <atomic>
#include "../base/NonCopyable.h"

namespace gateway {

struct Session {
  int64_t connId;
  std::string account;
  bool loggedIn;
  uint64_t loginTime;
  uint64_t lastHeartbeat;
};

class SessionManager : NonCopyable {
 public:
  static SessionManager& Instance();

  bool AddSession(int64_t connId);
  bool RemoveSession(int64_t connId);
  bool Login(int64_t connId, const std::string& account);
  bool Logout(int64_t connId);
  bool IsLoggedIn(int64_t connId) const;
  void UpdateHeartbeat(int64_t connId);
  Session* GetSession(int64_t connId);

  size_t SessionCount() const;

 private:
  SessionManager() = default;
  mutable std::mutex mutex_;
  std::map<int64_t, Session> sessions_;
};

}

#endif  // HELICOPTER_SESSIONMANAGER_H
