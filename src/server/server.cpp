#include "server/server.hpp"
#include "server/middleware.hpp"
#include "spdlog/spdlog.h"
#include <expected>

namespace insights::server {

core::Result<std::unique_ptr<glz::http_server<false>>>
initServer(std::string Address, int Port) {
  auto Server{std::make_unique<glz::http_server<false>>()};

  spdlog::info("🧊ICICLE Insights Server🧊");
  Server->bind(Address, Port).with_signals();
  spdlog::info("Binding to Address: {}, Port: {}.", Address, Port);

  Server->wrap(middleware::createLoggingMiddleware());
  return Server;
}

core::Result<void> startServer(glz::http_server<false> &Server,
                               std::optional<int> Workers) {
  try {
    spdlog::info("Starting {} worker threads.",
                 Workers.has_value() ? Workers.value() : 1);
    Server.start(Workers.value_or(1));
    Server.wait_for_signal();
    return {};
  } catch (...) {
    Server.stop();
    spdlog::error("Error running server");
    return std::unexpected<core::Error>("Error running server.");
  }
}
} // namespace insights::server
