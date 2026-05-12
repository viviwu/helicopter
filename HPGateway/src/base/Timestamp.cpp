/**
 **************************************************************************
 * @file    :Timestamp.cpp
 * @author  :viviwu
 * @brief   :时间戳实现
 * @version :0.1
 * @date    :5/12/26 PM3:40
 * **************************************************************************
 */

#include "Timestamp.h"
#include <sys/time.h>
#include <stdio.h>

namespace gateway {

Timestamp Timestamp::Now() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  int64_t seconds = tv.tv_sec;
  return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

Timestamp Timestamp::Invalid() {
  return Timestamp();
}

std::string Timestamp::ToString() const {
  char buf[32] = {0};
  int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
  int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
  snprintf(buf, sizeof(buf), "%lld.%06lld",
           static_cast<long long>(seconds),
           static_cast<long long>(microseconds));
  return buf;
}

std::string Timestamp::ToFormattedString(bool showMicroseconds) const {
  char buf[64] = {0};
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
  int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
  struct tm tm_time;
  localtime_r(&seconds, &tm_time);
  if (showMicroseconds) {
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
             microseconds);
  } else {
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
  }
  return buf;
}

}
