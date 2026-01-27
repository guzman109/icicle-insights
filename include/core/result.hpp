#pragma once
#include <expected>
#include <string>

namespace insights::core {

struct Error {
  std::string Message;
};

template <typename T> using Result = std::expected<T, Error>;

} // namespace insights::core
