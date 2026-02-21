#pragma once
#include "insights/core/http.hpp"

#include <string_view>

namespace insights::server {

struct Response {
  std::string_view status;
  std::string_view data;
};
} // namespace insights::server
