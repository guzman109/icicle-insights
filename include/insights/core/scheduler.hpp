#pragma once
#include <asio/steady_timer.hpp>
#include <chrono>
#include <format>
#include <functional>
#include <memory>
#include <spdlog/spdlog.h>
#include <string_view>
#include <system_error>

namespace insights::core {

// Schedules a recurring task on an ASIO timer.
//
// The task fires after InitialDelay, then repeats every Interval.
// Timing for each run is logged automatically via spdlog.
//
// Parameters:
//   Timer        - Shared steady_timer bound to the application's io_context
//   Name         - Human-readable label used in log output (e.g. "GitHub sync")
//   InitialDelay - How long to wait before the first run
//   Interval     - How long to wait between subsequent runs
//   Task         - The work to perform on each firing
//
// Usage:
//   core::scheduleRecurringTask(
//       GitHubSyncTimer,
//       "GitHub sync",
//       std::chrono::seconds(0),
//       std::chrono::weeks(2),
//       [&] { github::tasks::syncStats(*TasksDatabase, *Config); });
void scheduleRecurringTask(
    std::shared_ptr<asio::steady_timer> Timer,
    std::string_view Name,
    std::chrono::steady_clock::duration InitialDelay,
    std::chrono::steady_clock::duration Interval,
    std::function<void()> Task
) {
  auto Handler =
      std::make_shared<std::function<void(const std::error_code &)>>();

  *Handler = [Handler, Timer, Name, Interval, Task](const std::error_code &Ec) {
    if (Ec) {
      spdlog::error("[{}] Timer error: {}", Name, Ec.message());
      return;
    }

    spdlog::info("[{}]: Starting...", Name);

    auto Start = std::chrono::steady_clock::now();
    Task();
    auto Stop = std::chrono::steady_clock::now();
    auto Duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(Stop - Start);

    spdlog::info("[{}] completed in {}ms.", Name, Duration.count());

    Timer->expires_after(Interval);
    Timer->async_wait(*Handler);
  };

  Timer->expires_after(InitialDelay);
  Timer->async_wait(*Handler);
}

} // namespace insights::core
