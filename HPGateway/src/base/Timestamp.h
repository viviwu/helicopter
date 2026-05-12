/**
 **************************************************************************
 * @file    :Timestamp.h
 * @author  :viviwu
 * @brief   :时间戳封装
 * @version :0.1
 * @date    :5/12/26 PM3:40
 * **************************************************************************
 */

#ifndef HELICOPTER_TIMESTAMP_H
#define HELICOPTER_TIMESTAMP_H

#include <cstdint>
#include <string>

namespace gateway {

class Timestamp {
 public:
  Timestamp() : microSecondsSinceEpoch_(0) {}
  explicit Timestamp(int64_t microSecondsSinceEpoch)
      : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

  static Timestamp Now();
  static Timestamp Invalid();

  bool Valid() const { return microSecondsSinceEpoch_ > 0; }

  int64_t MicroSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
  int64_t MilliSecondsSinceEpoch() const {
    return microSecondsSinceEpoch_ / 1000;
  }
  time_t SecondsSinceEpoch() const {
    return static_cast<time_t>(microSecondsSinceEpoch_ / 1000000);
  }

  std::string ToString() const;
  std::string ToFormattedString(bool showMicroseconds = true) const;

  static const int kMicroSecondsPerSecond = 1000000;

 private:
  int64_t microSecondsSinceEpoch_;
};

inline bool operator<(Timestamp lhs, Timestamp rhs) {
  return lhs.MicroSecondsSinceEpoch() < rhs.MicroSecondsSinceEpoch();
}
inline bool operator>(Timestamp lhs, Timestamp rhs) {
  return lhs.MicroSecondsSinceEpoch() > rhs.MicroSecondsSinceEpoch();
}
inline bool operator==(Timestamp lhs, Timestamp rhs) {
  return lhs.MicroSecondsSinceEpoch() == rhs.MicroSecondsSinceEpoch();
}
inline double TimeDifference(Timestamp high, Timestamp low) {
  int64_t diff = high.MicroSecondsSinceEpoch() - low.MicroSecondsSinceEpoch();
  return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}
inline Timestamp AddTime(Timestamp timestamp, double seconds) {
  int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
  return Timestamp(timestamp.MicroSecondsSinceEpoch() + delta);
}

}

#endif  // HELICOPTER_TIMESTAMP_H
