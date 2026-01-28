#include "git/router.hpp"
#include <spdlog/spdlog.h>

namespace insights::git {
core::Result<void> registerRoutes(glz::http_router &Router,
                                  std::shared_ptr<db::Database> &Database) {
  spdlog::debug("Registering git platforms routes");
  auto Res = registerPlatformsRoutes(Router, Database);
  if (!Res) {
    spdlog::error("Failed to register platforms routes: {}", Res.error().Message);
    return Res;
  }

  spdlog::debug("Registering git accounts routes");
  Res = registerAccountsRoutes(Router, Database);
  if (!Res) {
    spdlog::error("Failed to register accounts routes: {}", Res.error().Message);
    return Res;
  }

  spdlog::debug("Registering git repos routes");
  Res = registerReposRoutes(Router, Database);
  if (!Res) {
    spdlog::error("Failed to register repos routes: {}", Res.error().Message);
    return Res;
  }

  spdlog::info("Successfully registered all git routes");
  return core::Result<void>{};
}
} // namespace insights::git
