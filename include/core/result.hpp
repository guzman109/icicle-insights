#pragma once
#include <expected>
#include <string>

namespace insights::core {

struct Error {
  std::string Message;
};

// Result<T> type alias removed - use std::expected<T, core::Error> directly

} // namespace insights::core
