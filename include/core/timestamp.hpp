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
} // namespace insights::git::models::detail
