/**
 **************************************************************************
 * @file    :Logger.h
 * @author  :viviwu
 * @brief   :日志封装（基于spdlog）
 * @version :0.1
 * @date    :5/12/26 PM3:40
 * **************************************************************************
 */

#ifndef HELICOPTER_LOGGER_H
#define HELICOPTER_LOGGER_H

#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace gateway {

class Logger {
 public:
  static Logger& Instance();

  bool Init(const std::string& name = "gateway",
            const std::string& level = "info");

  std::shared_ptr<spdlog::logger> GetLogger() { return logger_; }

 private:
  Logger() = default;
  std::shared_ptr<spdlog::logger> logger_;
};

}

#define LOG_TRACE(...) ::gateway::Logger::Instance().GetLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::gateway::Logger::Instance().GetLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)  ::gateway::Logger::Instance().GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)  ::gateway::Logger::Instance().GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::gateway::Logger::Instance().GetLogger()->error(__VA_ARGS__)
#define LOG_FATAL(...) ::gateway::Logger::Instance().GetLogger()->critical(__VA_ARGS__)

#endif  // HELICOPTER_LOGGER_H
