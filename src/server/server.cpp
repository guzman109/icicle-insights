#include "insights/server/server.hpp"
#include "insights/server/middleware.hpp"
#include "spdlog/spdlog.h"
#include <expected>

namespace insights::server {

std::expected<std::unique_ptr<glz::http_server<false>>, core::Error>
initServer(std::string Address, int Port) {
  auto Server{std::make_unique<glz::http_server<false>>()};

  spdlog::info("🧊ICICLE Insights Server🧊");
  Server->bind(Address, Port).with_signals();
  spdlog::info("Binding to Address: {}, Port: {}.", Address, Port);

  Server->wrap(middleware::createLoggingMiddleware());
  return Server;
}

std::expected<void, core::Error> startServer(glz::http_server<false> &Server,
                                             std::string Address, int Port,
                                             std::optional<int> Workers) {
  try {
    Server.start(Workers.value_or(1));
    spdlog::info("Server ready and listening on http://{}:{}", Address, Port);
    Server.wait_for_signal();
    return {};
  } catch (...) {
    Server.stop();
    spdlog::error("Error running server");
    return std::unexpected<core::Error>("Error running server.");
  }
}
} // namespace insights::server
