#include <glaze/net/http_router.hpp>
#include <memory>

#include "core/config.hpp"
#include "db/db.hpp"
#include "git/router.hpp"
#include "server/server.hpp"
#include "spdlog/spdlog.h"

int main() {
  auto Server = insights::server::initServer("0.0.0.0", 3000);
  glz::http_router Router;

  auto Config = insights::core::Config::load();
  if (!Config) {
    spdlog::error(Config.error().Message);
    return 1;
  }

  spdlog::info("Connecting to database.");
  auto Database = insights::db::Database::connect(Config->DatabaseUrl);
  if (!Database) {
    spdlog::error(Database.error().Message);
    return 1;
  }

  spdlog::info("Registering routes:");
  spdlog::info("GitRoutes");

  glz::http_router GitRouter;

  if (!insights::git::registerRoutes(GitRouter, Database.value())) {
    spdlog::error("Failed registering git routes.");
  }

  Server.value()->mount("/api/git", GitRouter);

  auto ServerResult = insights::server::startServer(*Server.value());
  if (ServerResult) {
    spdlog::error("{}", ServerResult.error().Message);
    return 1;
  }
}
