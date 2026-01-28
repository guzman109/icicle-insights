# Background Task Scheduling Guide

This guide covers how to run periodic background tasks (like updating Git metrics) while the HTTP server is running.

## Requirements

- Run `git::tasks::runAll()` periodically (every 2 weeks)
- Non-blocking - server must remain responsive
- Graceful shutdown support
- Error handling and logging

## Architecture Options

### Option 1: ASIO Timers (Recommended)

**Pros:**
- Integrates with existing ASIO event loop
- Non-blocking, single-threaded
- Clean shutdown via `io_context::stop()`
- No thread synchronization needed
- Consistent error handling with the rest of the app

**Cons:**
- Slightly more complex than simple thread
- Tasks run on the same thread as HTTP handlers (but async)

**When to use:** Default choice for most use cases. Best fit for this project.

### Option 2: Separate Thread with Timer

**Pros:**
- Simple to implement
- Tasks run on dedicated thread (won't block event loop)

**Cons:**
- Requires thread synchronization for database access
- Harder to test with shorter intervals
- No automatic shutdown handling
- More complex resource management

**When to use:** If tasks are CPU-intensive and would block the event loop significantly.

### Option 3: External Scheduler (Cron/Systemd Timer)

**Pros:**
- Completely separate from application
- Survives app restarts
- Standard Unix pattern
- No in-process overhead

**Cons:**
- Requires deployment configuration
- Less visibility into task status
- Harder to test locally
- Needs separate auth/config

**When to use:** Production systems with ops team, or if you need tasks to run even when server is down.

## Recommended Implementation: ASIO Timer

### Step 1: Create Scheduler Class

Create `include/git/scheduler.hpp`:

```cpp
#pragma once
#include "core/result.hpp"
#include "db/db.hpp"
#include <asio/steady_timer.hpp>
#include <memory>
#include <chrono>

namespace insights::git {

class TaskScheduler {
public:
  // Constructor takes io_context and database connection
  TaskScheduler(asio::io_context &Io, std::shared_ptr<db::Database> Db);

  // Start the scheduler with the given interval
  void start(std::chrono::seconds Interval);

  // Stop the scheduler gracefully
  void stop();

  // Check if scheduler is running
  bool isRunning() const { return Running; }

private:
  void scheduleNext();
  void onTimerExpired(const std::error_code &Error);

  asio::steady_timer Timer;
  std::shared_ptr<db::Database> Db;
  std::chrono::seconds Interval;
  bool Running;
};

} // namespace insights::git
```

### Step 2: Implement Scheduler Logic

Create `src/git/scheduler.cpp`:

```cpp
#include "git/scheduler.hpp"
#include "git/tasks.hpp"
#include <spdlog/spdlog.h>

namespace insights::git {

TaskScheduler::TaskScheduler(asio::io_context &Io, std::shared_ptr<db::Database> Db)
    : Timer(Io), Db(std::move(Db)), Running(false) {}

void TaskScheduler::start(std::chrono::seconds Interval) {
  if (Running) {
    spdlog::warn("TaskScheduler already running, ignoring start request");
    return;
  }

  this->Interval = Interval;
  Running = true;

  spdlog::info("Starting task scheduler (interval: {} seconds)", Interval.count());

  // Run tasks immediately on startup, then schedule periodic runs
  auto Result = tasks::runAll(*Db);
  if (!Result) {
    spdlog::error("Initial task execution failed: {}", Result.error().Message);
  } else {
    spdlog::info("Initial tasks completed successfully");
  }

  scheduleNext();
}

void TaskScheduler::stop() {
  if (!Running) return;

  spdlog::info("Stopping task scheduler");
  Running = false;
  Timer.cancel();
}

void TaskScheduler::scheduleNext() {
  if (!Running) return;

  Timer.expires_after(Interval);
  Timer.async_wait([this](const std::error_code &Error) {
    onTimerExpired(Error);
  });
}

void TaskScheduler::onTimerExpired(const std::error_code &Error) {
  if (Error == asio::error::operation_aborted) {
    // Timer was cancelled (clean shutdown)
    spdlog::debug("Task timer cancelled");
    return;
  }

  if (Error) {
    spdlog::error("Timer error: {}", Error.message());
    return;
  }

  if (!Running) return;

  spdlog::info("Running scheduled git tasks...");

  auto Result = tasks::runAll(*Db);
  if (!Result) {
    spdlog::error("Task execution failed: {}", Result.error().Message);
  } else {
    spdlog::info("Tasks completed successfully");
  }

  // Schedule next run
  scheduleNext();
}

} // namespace insights::git
```

### Step 3: Add to CMakeLists.txt

Add the new source file to your executable:

```cmake
add_executable(insights
    src/insights.cpp
    src/git/router.cpp
    src/git/tasks.cpp
    src/git/scheduler.cpp  # Add this line
    # ... other sources
)
```

### Step 4: Integrate with Server

In `src/insights.cpp` (or wherever you initialize the server):

```cpp
#include "git/scheduler.hpp"

int main() {
  // ... existing setup code ...

  asio::io_context Io;
  auto Db = std::make_shared<db::Database>(/* connection */);

  // ... HTTP server setup ...

  // Create and start task scheduler
  auto Scheduler = std::make_unique<git::TaskScheduler>(Io, Db);

  // For production: 2 weeks = 14 days * 24 hours * 3600 seconds
  using namespace std::chrono_literals;
  Scheduler->start(std::chrono::seconds(14 * 24 * 60 * 60));

  // For testing: run every 5 minutes
  // Scheduler->start(5min);

  // Run the event loop (blocks until shutdown)
  Io.run();

  // Clean shutdown
  Scheduler->stop();

  return 0;
}
```

## Configuration

Add scheduler settings to your config:

```cpp
// In core/config.hpp or wherever config lives
struct Config {
  // ... existing fields ...

  // Task scheduler interval in seconds (default: 2 weeks)
  int TaskSchedulerInterval = 14 * 24 * 60 * 60;
};
```

Load from environment or config file:

```bash
# .env
TASK_SCHEDULER_INTERVAL=1209600  # 2 weeks in seconds
```

## Testing

### Test with Short Interval

```cpp
// In test or dev mode
Scheduler->start(std::chrono::seconds(10));  // Every 10 seconds
```

### Manual Trigger Endpoint

Add an admin endpoint to trigger tasks manually:

```cpp
// In router setup
Router.post("/admin/tasks/run", [Db](auto &Req, auto &Res) {
  // TODO: Add authentication check

  auto Result = git::tasks::runAll(*Db);
  if (!Result) {
    Res.status(500).send(Result.error().Message);
  } else {
    Res.send("Tasks completed successfully");
  }
});
```

### Unit Tests

```cpp
// Test that scheduler starts and stops cleanly
TEST(TaskScheduler, StartStop) {
  asio::io_context Io;
  auto Db = std::make_shared<db::Database>(/* mock */);

  git::TaskScheduler Scheduler(Io, Db);
  EXPECT_FALSE(Scheduler.isRunning());

  Scheduler.start(std::chrono::seconds(60));
  EXPECT_TRUE(Scheduler.isRunning());

  Scheduler.stop();
  EXPECT_FALSE(Scheduler.isRunning());
}
```

## Error Handling

Tasks may fail for various reasons:
- Database connection issues
- API rate limits
- Network errors

The scheduler logs errors but continues running. Individual task functions should:
1. Return `Result<void>` with descriptive errors
2. Log detailed error information
3. Not throw exceptions across ASIO boundaries

## Monitoring

Add metrics/logging:

```cpp
void onTimerExpired(const std::error_code &Error) {
  // ... existing code ...

  auto StartTime = std::chrono::steady_clock::now();
  auto Result = tasks::runAll(*Db);
  auto Duration = std::chrono::steady_clock::now() - StartTime;

  spdlog::info("Task execution took {}ms",
    std::chrono::duration_cast<std::chrono::milliseconds>(Duration).count());

  // TODO: Send metrics to monitoring system
}
```

## Performance Considerations

### Event Loop Blocking

If tasks take a long time (>1 second), they'll block HTTP requests. Solutions:

1. **Post to thread pool** (if ASIO has one configured)
2. **Use async database operations** (libpqxx async API)
3. **Break into smaller chunks** with `io_context::post()`

Example with chunking:

```cpp
void TaskScheduler::onTimerExpired(const std::error_code &Error) {
  // ... error checks ...

  // Post actual work to avoid blocking timer
  asio::post(Timer.get_executor(), [this]() {
    auto Result = tasks::runAll(*Db);
    // ... handle result ...
  });

  // Schedule next run immediately
  scheduleNext();
}
```

### Database Connection Pool

Ensure `Database` class uses connection pooling to handle concurrent requests while tasks are running.

## Deployment

### Development

```bash
# .env
TASK_SCHEDULER_INTERVAL=300  # 5 minutes for testing
```

### Production

```bash
# .env
TASK_SCHEDULER_INTERVAL=1209600  # 2 weeks

# Or disable in-app scheduler and use cron:
TASK_SCHEDULER_ENABLED=false
```

### Graceful Shutdown

The scheduler stops automatically when `io_context::stop()` is called (e.g., on SIGTERM/SIGINT). Ensure your main function handles signals:

```cpp
#include <csignal>

asio::io_context Io;
asio::signal_set Signals(Io, SIGINT, SIGTERM);

Signals.async_wait([&](auto, auto) {
  spdlog::info("Shutdown signal received");
  Io.stop();
});

// ... start scheduler ...

Io.run();
```

## Alternative: Cron-based Approach

If you decide on external scheduling instead:

### Create CLI Command

```cpp
// src/cli.cpp
int main(int argc, char *argv[]) {
  if (argc > 1 && std::string(argv[1]) == "run-tasks") {
    auto Db = db::Database(/* connection */);
    auto Result = git::tasks::runAll(Db);
    return Result ? 0 : 1;
  }
  // ... other commands ...
}
```

### Cron Entry

```bash
# Run every 2 weeks on Sunday at 2am
0 2 */14 * 0 /path/to/insights run-tasks >> /var/log/insights-tasks.log 2>&1
```

### Systemd Timer

```ini
# /etc/systemd/system/insights-tasks.timer
[Unit]
Description=ICICLE Insights Task Runner

[Timer]
OnCalendar=biweekly
Persistent=true

[Install]
WantedBy=timers.target
```

```ini
# /etc/systemd/system/insights-tasks.service
[Unit]
Description=Run ICICLE Insights Git Tasks

[Service]
Type=oneshot
ExecStart=/usr/local/bin/insights run-tasks
```

## Summary

**For ICICLE Insights, use Option 1 (ASIO Timer)** because:
- ✅ Simple integration with existing ASIO server
- ✅ No threading complexity
- ✅ Easy to test with short intervals
- ✅ Graceful shutdown built-in
- ✅ Consistent with codebase architecture

The implementation adds ~100 lines of code and integrates cleanly with your existing patterns.
