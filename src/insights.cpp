#include "insights/core/config.hpp"
#include "insights/core/logging.hpp"
#include "insights/core/scheduler.hpp"
#include "insights/db/db.hpp"
#include "insights/github/routes.hpp"
#include "insights/github/tasks.hpp"
#include "insights/server/middleware/logging.hpp"
#include "insights/core/routes.hpp"

#include "spdlog/spdlog.h"

#include <asio/any_io_executor.hpp>
#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <glaze/net/http_router.hpp>
#include <glaze/net/http_server.hpp>
#include <memory>
#include <string_view>
#include <system_error>
#include <thread>

int main() {
  // Use ASIO IO Context for background tasks.
  auto IOContext = std::make_shared<asio::io_context>();

  // Pull Environment Variables.
  auto Config = insights::core::Config::load();
  if (!Config) {
    spdlog::error(Config.error().Message);
    return 1;
  }

  // Configure logging — server logger becomes default, one file per component
  insights::core::setupLogging(*Config);
  insights::core::createLogger("github_sync", *Config);

  spdlog::debug(
      "Loaded config - Host: {}, Port: {}", Config->Host, Config->Port
  );

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
  insights::core::registerCoreRoutes(Router, ServerDatabase.value());
  spdlog::info("GitHubRoutes");
  glz::http_router GitHubRouter;

  if (!insights::github::registerRoutes(GitHubRouter, ServerDatabase.value())) {
    spdlog::error("Failed registering git routes.");
  }

  // Mount the routers
  Server.mount("/", Router);
  Server.mount("/api/github", GitHubRouter);

  // Start The Server (0 Worker Threads so we can run with ASIO shared IO
  // Context)
  Server.start(0);

  // Tasks

  // GitHub Metrics Sync Task

  // Query DB for seconds until the next scheduled run.
  // Returns nullopt on first-ever run (no row yet); negative if overdue.
  auto DelayResult = ServerDatabase.value()->querySecondsUntilNextRun("GitHubSync");
  auto SecondsUntilNext = (DelayResult && *DelayResult)
      ? std::max(**DelayResult, 0LL)
      : 0LL;
  auto InitialDelay = std::chrono::seconds(SecondsUntilNext);

  // Set Task Timer
  auto GitHubSyncTimer = std::make_shared<asio::steady_timer>(*IOContext);

  // Schedule the task
  spdlog::info("GitHubSync ready and running every 2 weeks on Saturday.");
  insights::core::scheduleRecurringTask(
      GitHubSyncTimer, "GitHubSync", InitialDelay, std::chrono::weeks(2), [Config] {
        insights::github::tasks::syncStats(*Config);
      }
  );

  // Start All Threads (Server + Tasks)
  // Start the Thread pool.
  std::vector<std::thread> Threads;
  const size_t NumThreads = std::thread::hardware_concurrency();
  Threads.reserve(NumThreads);

  spdlog::info(
      "Server ready and listening on http://{}:{}", Config->Host, Config->Port
  );

  spdlog::info("Sharing {} threads.", NumThreads);

  asio::signal_set Signals(*IOContext, SIGINT, SIGTERM);
  Signals.async_wait([&](const std::error_code &, int) {
    spdlog::info("Shutdown signal received.");
    Server.stop();
    IOContext->stop();
    spdlog::shutdown();
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
