#pragma once
#include "glaze/net/http.hpp"
#include "glaze/net/http_server.hpp"

#include "spdlog/spdlog.h"

namespace insights::server::middleware {

inline auto createLoggingMiddleware() {
  return [](const glz::request &Request,
            glz::response &Response,
            const auto &Next) {
    auto Start = std::chrono::steady_clock::now();
    // Handler Execution
    Next();
    auto Duration = std::chrono::steady_clock::now() - Start;
    // Log with status
    spdlog::info(
        "[{}] {} {} {}ms",
        glz::to_string(Request.method),
        Request.path,
        Response.status_code,
        std::chrono::duration_cast<std::chrono::milliseconds>(Duration).count()
    );
  };
}

} // namespace insights::server::middleware
