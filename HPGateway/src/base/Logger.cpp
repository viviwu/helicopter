/**
 **************************************************************************
 * @file    :Logger.cpp
 * @author  :viviwu
 * @brief   :日志实现
 * @version :0.1
 * @date    :5/12/26 PM3:40
 * **************************************************************************
 */

#include "Logger.h"
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace gateway {

Logger& Logger::Instance() {
  static Logger instance;
  return instance;
}

bool Logger::Init(const std::string& name, const std::string& level) {
  try {
    spdlog::init_thread_pool(8192, 1);
    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/" + name + ".log", 1048576 * 10, 3);

    std::vector<spdlog::sink_ptr> sinks{stdout_sink, file_sink};
    logger_ = std::make_shared<spdlog::async_logger>(
        name, sinks.begin(), sinks.end(), spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);

    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");

    if (level == "trace")
      logger_->set_level(spdlog::level::trace);
    else if (level == "debug")
      logger_->set_level(spdlog::level::debug);
    else if (level == "info")
      logger_->set_level(spdlog::level::info);
    else if (level == "warn")
      logger_->set_level(spdlog::level::warn);
    else if (level == "error")
      logger_->set_level(spdlog::level::err);
    else if (level == "fatal")
      logger_->set_level(spdlog::level::critical);
    else
      logger_->set_level(spdlog::level::info);

    return true;
  } catch (...) {
    return false;
  }
}

}
