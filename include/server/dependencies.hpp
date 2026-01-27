#pragma once

#include "glaze/net/http_router.hpp"
#include <regex>
#include <string>
#include <string_view>

namespace insights::server::dependencies {
inline glz::param_constraint uuidConstraint() {
  glz::param_constraint Uuid{
      .description = "Must be a valid UUID",
      .validation = [](std::string_view Value) {
        std::regex UuidRegex(
            R"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})");
        return std::regex_match(std::string(Value), UuidRegex);
      }};
  return Uuid;
}
} // namespace insights::server::dependencies
