# Database Reconnect Guide

How the `Database` class handles lost connections: automatic retry with exponential backoff.

## Overview

A database connection is a TCP socket. Like any network resource, it can be dropped by firewalls, load balancers, or a Postgres restart — even mid-operation. Rather than propagating that failure immediately to the caller, the `Database` class tries to re-establish the connection and replay the operation transparently.

All database operations go through `withRetry`, a private template method that catches connection errors, waits, calls `reconnect()`, and tries again — up to `MaxRetries` times. Non-connection errors (SQL syntax errors, constraint violations) fail immediately without retrying, because replaying them won't help.

```
Operation fails
      │
      ▼
pqxx::broken_connection   ──► retry with backoff
std::exception (connection keywords) ──► retry with backoff
std::exception (other)    ──► fail immediately, no retry
```

## Retry Parameters

Defined as private constants in `include/insights/db/db.hpp`:

| Constant | Value | Purpose |
|----------|-------|---------|
| `MaxRetries` | `3` | Maximum number of retry attempts |
| `BaseDelay` | `1s` | Starting backoff delay |

## Backoff Schedule

Retrying immediately after a failure usually doesn't help — if the database just restarted, it needs a moment to recover. Waiting longer on each successive attempt (exponential backoff) gives it progressively more time without waiting forever.

`backoffDelay(Attempt)` computes `BaseDelay × 2^Attempt`, capped at 30 seconds:

| Attempt | Delay |
|---------|-------|
| 0 (first retry) | 1s |
| 1 | 2s |
| 2 | 4s |
| 3+ | capped at 30s |

```cpp
static std::chrono::seconds backoffDelay(int Attempt) {
  auto Delay = BaseDelay.count() * (1LL << Attempt);
  return std::chrono::seconds(std::min(Delay, 30LL));
}
```

## How withRetry Works

The retry logic is centralised in one private method so individual operations stay simple — they just describe what query to run, not how to handle failures. The C++ template lets `withRetry` work with any return type, including `void` (handled via `if constexpr`).

There are two separate catch blocks intentionally: `pqxx::broken_connection` is a specific libpqxx type for TCP-level drops (the most reliable signal), while the `std::exception` catch is a fallback for other libraries or Postgres versions that may surface connection loss differently. Checking the message for keywords like `"connection"` and `"broken"` is the fallback heuristic.

Every public CRUD method wraps its lambda in `withRetry`:

```cpp
template <core::DbEntity T>
std::expected<T, core::Error> get(std::string_view Id) {
  return withRetry("Database::get", [this, Id]() -> T {
    pqxx::read_transaction Tx(Cx);
    // ... execute query
  });
}
```

Inside `withRetry`, the loop:

1. Calls the operation lambda.
2. On `pqxx::broken_connection`: logs a warning, sleeps for `backoffDelay(Attempt)`, calls `reconnect()`, and retries.
3. On `std::exception` with connection-related keywords (`"connection"`, `"lost"`, `"broken"`): same as above.
4. On any other `std::exception`: logs an error and returns immediately with `std::unexpected`.
5. After `MaxRetries` exhausted: returns `std::unexpected(Error{"Exhausted retries"})`.

## reconnect()

`reconnect()` replaces the existing `pqxx::connection` with a fresh one using the stored connection string. This works because `pqxx::connection` is **move-assignable** — assigning a new connection to `Cx` transfers ownership of the new TCP socket and destroys the old (dead) one atomically. At no point is there zero connections.

`ConnString` is stored as a private member because `pqxx::connection` doesn't expose the original string after construction — without storing it yourself, reconnection would be impossible.

```cpp
bool reconnect() {
  spdlog::warn("Database::reconnect - Attempting to reconnect");
  try {
    Cx = pqxx::connection(ConnString);
    spdlog::info("Database::reconnect - Reconnected successfully");
    return true;
  } catch (const std::exception &Err) {
    spdlog::error("Database::reconnect - Failed: {}", Err.what());
    return false;
  }
}
```

The connection string is stored at construction time so it is always available for reconnection without any external input.

## Initial Connection

`Database::connect()` is a static factory that creates the initial connection. It does **not** retry — if the database is unreachable at startup, the server exits. This is intentional: startup failure is a configuration problem (wrong URL, database not running), not a transient blip. Retrying would just delay the error message.

```cpp
// src/insights.cpp
auto ServerDatabase = db::Database::connect(Config->DatabaseUrl);
if (!ServerDatabase) {
  spdlog::error(ServerDatabase.error().Message);
  return 1;
}
```

Background tasks follow the same pattern, creating their own connection at the start of each run (see [background-tasks.md](background-tasks.md)).

## Current Connection Topology

The server currently uses two database access patterns:

| Instance | Owner | Purpose |
|----------|-------|---------|
| `ServerDatabase` | HTTP route handlers | Shared server-side connection used by request handlers |
| Task-local connection | Background sync tasks | Fresh connection opened at the start of each sync run |

The task-local connection is a good fit for the background sync job: it opens at the start of the run and closes at scope exit (RAII), which avoids holding an idle connection open for the two weeks between runs.

The HTTP-side connection is more problematic. The current implementation shares one `ServerDatabase` across request handlers, and that `Database` owns a single `pqxx::connection`. Since `pqxx::connection` is not thread-safe, this is a known concurrency risk in the current server design when multiple requests hit the database at the same time.

So the reconnect logic described in this guide is accurate, but the surrounding connection ownership model is **not** yet ideal. A future connection-per-request or connection-pool design would make the retry behavior safer in a multithreaded server.

## Log Output

All reconnect activity is logged through spdlog. Set `LOG_LEVEL=debug` in `.env` to see full detail:

```
[warn]  Database::get - Connection lost (attempt 1/3), retrying in 1s: ...
[warn]  Database::reconnect - Attempting to reconnect
[info]  Database::reconnect - Reconnected successfully
[warn]  Database::get - Connection lost (attempt 2/3), retrying in 2s: ...
...
[error] Database::get - Connection lost, no retries left: ...
```

## Operational Caveat

The current retry implementation uses `std::this_thread::sleep_for(...)` during backoff. In the current server architecture, that means a worker thread can be blocked during reconnect waits. This is acceptable for a small system, but it is not free: repeated reconnect attempts can reduce request-serving capacity while the backoff is in progress.

A future async backoff strategy or dedicated DB executor would improve this.
