#include <asio/any_io_executor.hpp>
#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <glaze/net/http_router.hpp>
#include <glaze/net/http_server.hpp>
#include <memory>
#include <system_error>
#include <thread>

#include "insights/core/config.hpp"
#include "insights/db/db.hpp"
#include "insights/git/router.hpp"
#include "insights/git/tasks.hpp"
#include "insights/server/middleware.hpp"
#include "insights/server/routes.hpp"
#include "insights/server/server.hpp"
#include "spdlog/spdlog.h"

int main() {
  auto IOContext = std::make_shared<asio::io_context>();
  // Configure log level
  auto *LogLevelEnv = std::getenv("LOG_LEVEL");
  if (LogLevelEnv != nullptr) {
    std::string LogLevel = LogLevelEnv;
    if (LogLevel == "trace") {
      spdlog::set_level(spdlog::level::trace);
    } else if (LogLevel == "debug") {
      spdlog::set_level(spdlog::level::debug);
    } else if (LogLevel == "info") {
      spdlog::set_level(spdlog::level::info);
    } else if (LogLevel == "warn") {
      spdlog::set_level(spdlog::level::warn);
    } else if (LogLevel == "error") {
      spdlog::set_level(spdlog::level::err);
    }
  } else {
    {
      spdlog::set_level(spdlog::level::info);
    }
  }

  auto Config = insights::core::Config::load();
  if (!Config) {
    spdlog::error(Config.error().Message);
    return 1;
  }

  spdlog::debug("Loaded config - Host: {}, Port: {}", Config->Host,
                Config->Port);

  // auto Server = insights::server::initServer(Config->Host, Config->Port);
  // Initialize Insights HTTP Server
  auto Server{glz::http_server<false>(IOContext)};

  spdlog::info("🧊ICICLE Insights Server🧊");
  Server.bind(Config->Host, Config->Port);
  spdlog::info("Binding to Address: {}, Port: {}.", Config->Host, Config->Port);

  // Register Middleware
  Server.wrap(insights::server::middleware::createLoggingMiddleware());

  // Register Sever Database Connection
  spdlog::info("Connecting to database.");
  auto ServerDatabase = insights::db::Database::connect(Config->DatabaseUrl);
  if (!ServerDatabase) {
    spdlog::error(ServerDatabase.error().Message);
    return 1;
  }

  // Register Routes
  glz::http_router Router;
  spdlog::info("Registering routes:");
  insights::server::registerCoreRoutes(Router, ServerDatabase.value());
  spdlog::info("GitRoutes");
  glz::http_router GitRouter;

  if (!insights::git::registerRoutes(GitRouter, ServerDatabase.value())) {
    spdlog::error("Failed registering git routes.");
  }

  // Mount the routers
  Server.mount("/", Router);
  Server.mount("/api/git", GitRouter);

  // Start The Server (0 Worker Threads so we can run with ASIO shared IO
  // Context)
  Server.start(0);

  // Tasks

  // GitHub Metrics Sync Task

  // Set Task Timer
  auto GitHubSyncTimer = std::make_shared<asio::steady_timer>(*IOContext);

  // Needs to fire every other Sunday
  auto Now = std::chrono::system_clock::now();
  auto Today = std::chrono::floor<std::chrono::days>(Now);
  std::chrono::weekday CurrentDay(Today);

  auto DaysUntilSunday = (7 - CurrentDay.c_encoding()) % 7;
  auto InitialDelay = std::chrono::days(DaysUntilSunday);
  // auto InitialDelay = std::chrono::seconds(0);

  GitHubSyncTimer->expires_after(InitialDelay);

  // Register Database Connection for Tasks
  spdlog::info("Connecting to database.");
  auto TasksDatabase = insights::db::Database::connect(Config->DatabaseUrl);
  if (!TasksDatabase) {
    spdlog::error(TasksDatabase.error().Message);
    return 1;
  }

  auto OnGitHubSync =
      std::make_shared<std::function<void(const std::error_code &)>>();
  // Test tasks module
  *OnGitHubSync = [OnGitHubSync, GitHubSyncTimer, Config,
                   TasksDatabase](const std::error_code &ErrorCode) {
    if (ErrorCode) {
      spdlog::error("Task failed with error: {}.", ErrorCode.message());
      return;
    }
    spdlog::info("GitHub tasks...");
    // Run Task
    auto TasksResult = insights::git::tasks::runAll(
        Config->GitHubToken, *TasksDatabase.value(), *Config);

    GitHubSyncTimer->expires_after(std::chrono::weeks(2));
    GitHubSyncTimer->async_wait(*OnGitHubSync);
  };

  GitHubSyncTimer->async_wait(*OnGitHubSync);

  // Start the Thread pool.
  std::vector<std::thread> Threads;
  const size_t NumThreads = std::thread::hardware_concurrency();
  Threads.reserve(NumThreads);

  spdlog::info("Server ready and listening on http://{}:{}", Config->Host,
               Config->Port);

  spdlog::info("Tasks ready and running every 2 weeks on Sunday.");
  spdlog::info("Sharing {} threads.", NumThreads);

  asio::signal_set Signals(*IOContext, SIGINT, SIGTERM);
  Signals.async_wait([&](const std::error_code &, int) {
    spdlog::info("Shutdown signal received.");
    Server.stop();
    IOContext->stop();
  });

  for (auto _ : std::views::iota(0uz, NumThreads)) {
    Threads.emplace_back([IOContext]() { IOContext->run(); });
  }

  for (auto &Thread : Threads) {
    if (Thread.joinable()) {
      Thread.join();
    }
  }

  return 0;
}
