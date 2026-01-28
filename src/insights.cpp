#include <glaze/net/http_router.hpp>
#include <memory>

#include "core/config.hpp"
#include "db/db.hpp"
#include "git/router.hpp"
#include "server/routes.hpp"
#include "server/server.hpp"
#include "spdlog/spdlog.h"

int main() {
  // Configure log level
  auto *LogLevelEnv = std::getenv("LOG_LEVEL");
  if (LogLevelEnv != nullptr) {
    std::string LogLevel = LogLevelEnv;
    if (LogLevel == "trace") spdlog::set_level(spdlog::level::trace);
    else if (LogLevel == "debug") spdlog::set_level(spdlog::level::debug);
    else if (LogLevel == "info") spdlog::set_level(spdlog::level::info);
    else if (LogLevel == "warn") spdlog::set_level(spdlog::level::warn);
    else if (LogLevel == "error") spdlog::set_level(spdlog::level::err);
  } else {
    spdlog::set_level(spdlog::level::info);
  }

  auto Config = insights::core::Config::load();
  if (!Config) {
    spdlog::error(Config.error().Message);
    return 1;
  }

  spdlog::debug("Loaded config - Host: {}, Port: {}", Config->Host, Config->Port);

  auto Server = insights::server::initServer(Config->Host, Config->Port);
  if (!Server) {
    spdlog::error("Failed to initialize server: {}", Server.error().Message);
    return 1;
  }

  glz::http_router Router;

  spdlog::info("Connecting to database.");
  auto Database = insights::db::Database::connect(Config->DatabaseUrl);
  if (!Database) {
    spdlog::error(Database.error().Message);
    return 1;
  }

  spdlog::info("Registering routes:");
  insights::server::registerCoreRoutes(Router, Database.value());

  spdlog::info("GitRoutes");
  glz::http_router GitRouter;

  if (!insights::git::registerRoutes(GitRouter, Database.value())) {
    spdlog::error("Failed registering git routes.");
  }

  Server.value()->mount("/", Router);
  Server.value()->mount("/api/git", GitRouter);

  auto ServerResult = insights::server::startServer(*Server.value(), Config->Host, Config->Port);
  if (!ServerResult) {
    spdlog::error("Failed to start server: {}", ServerResult.error().Message);
    return 1;
  }
  spdlog::info("Server stopped");
  return 0;
}
