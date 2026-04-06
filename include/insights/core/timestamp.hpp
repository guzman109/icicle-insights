#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace insights::core {
inline std::chrono::system_clock::time_point
parseTimestamp(const std::string &Str) {
  std::tm Tm = {};
  std::istringstream Ss(Str);
  Ss >> std::get_time(&Tm, "%Y-%m-%d %H:%M:%S");
  return std::chrono::system_clock::from_time_t(std::mktime(&Tm));
}

inline std::string
formatTimestamp(const std::chrono::system_clock::time_point &Timestamp) {
  auto Time = std::chrono::system_clock::to_time_t(Timestamp);
  std::tm Tm = {};
  localtime_r(&Time, &Tm);

  std::ostringstream Ss;
  Ss << std::put_time(&Tm, "%Y-%m-%d %H:%M:%S %z");
  return Ss.str();
}
} // namespace insights::core
