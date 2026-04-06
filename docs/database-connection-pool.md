# Database Connection Pool Design

How to replace the current shared HTTP database connection with a pool that is
safe under concurrent request handling.

---

## Problem

The current HTTP path creates one `ServerDatabase` at startup and shares that
same `std::shared_ptr<db::Database>` across all route handlers.

That looks convenient, but it means every request handler can end up touching
the same underlying `pqxx::connection` concurrently.

Today, the runtime model is:

- one shared `asio::io_context`
- `Server.start(0)`
- `hardware_concurrency()` worker threads calling `IOContext->run()`
- route lambdas capturing the same `ServerDatabase`

`std::shared_ptr` only makes ownership safe. It does **not** make
`pqxx::connection` safe for concurrent use.

So the current design has a real race:

- request A starts a transaction on connection `Cx`
- request B starts another transaction on the same `Cx`
- libpqxx / libpq state is now shared across threads

This can lead to undefined behavior, corrupted protocol state, failed
transactions, or intermittent crashes.

Background tasks already avoid this problem better: `syncStats()` opens a fresh
connection per run and keeps that connection local to the task execution.

---

## Goal

Keep the existing route structure and `DbTraits<T>` CRUD model, but make HTTP
request DB access safe under multi-threaded request handling.

The design should:

- prevent multiple threads from using the same `pqxx::connection` at once
- preserve the current `std::expected<..., core::Error>` style
- fit the current Glaze route-handler model cleanly
- allow reconnect/retry logic to stay centralized
- avoid over-serializing the whole server behind one DB mutex

---

## Why A Pool

There are four obvious designs.

### 1. One Shared Connection

Current approach.

Pros:
- simple startup wiring

Cons:
- unsafe with concurrent request handling
- one connection becomes a global contention point
- one broken connection affects every request path

Verdict:
- not acceptable for a multi-threaded server

### 2. New Connection Per Request

Each route opens a fresh `pqxx::connection`, uses it, then drops it.

Pros:
- correct and simple
- easy to reason about
- no shared mutable DB state across threads

Cons:
- connection setup overhead on every request
- more churn for Postgres and any proxy in front of it
- loses the benefit of reusing already-open connections

Verdict:
- good emergency fix
- probably not the best long-term production design here

### 3. `thread_local` Connection Per Worker Thread

Each worker thread lazily creates one connection and reuses it.

Pros:
- avoids one global shared connection
- lower overhead than per-request connections

Cons:
- more fragile lifecycle
- harder to reason about with reconnects and shutdown
- ties DB behavior to the current thread model
- less flexible if async patterns evolve later

Verdict:
- workable, but too coupled to the executor/thread layout

### 4. Connection Pool

The server owns `N` connections. Each request borrows one connection, uses it
exclusively, and returns it automatically.

Pros:
- safe under concurrency
- bounded DB resource usage
- less connection churn than per-request connect/disconnect
- decouples DB usage from exact worker thread count
- fits future rate limiting / backpressure better

Cons:
- more code than a single shared connection
- requires borrow/return API and waiting behavior

Verdict:
- best fit for this codebase

---

## Recommended Design

Introduce a `DatabasePool` that owns a fixed set of `db::Database` instances.

Each request:

1. borrows one pooled connection
2. runs DB work exclusively on that connection
3. returns it via RAII when the borrow object goes out of scope

The pool should be used by HTTP route handlers.

Background tasks can stay on their current per-run connection model for now.
That keeps the refactor smaller and avoids mixing long-running sync work with
request-path capacity.

---

## High-Level API

Proposed new types:

```cpp
namespace insights::db {

class DatabasePool;

class PooledConnection {
public:
  PooledConnection() = default;
  PooledConnection(PooledConnection&&) noexcept;
  auto operator=(PooledConnection&&) noexcept -> PooledConnection&;
  ~PooledConnection();

  auto operator->() -> Database*;
  auto operator*() -> Database&;
  explicit operator bool() const;

private:
  DatabasePool* Pool{nullptr};
  std::shared_ptr<Database> Connection;
};

class DatabasePool {
public:
  static std::expected<std::shared_ptr<DatabasePool>, core::Error>
  create(const std::string& ConnString, std::size_t PoolSize);

  std::expected<PooledConnection, core::Error> acquire();

private:
  // queue / mutex / condition_variable / available connections
};

} // namespace insights::db
```

Key idea:

- `PooledConnection` is move-only
- destructor returns the connection to the pool automatically
- route handlers never manage release manually

---

## Route Integration

Current route signatures accept a shared database object:

```cpp
void registerCoreRoutes(glz::http_router &Router, std::shared_ptr<db::Database> Database);

auto registerRoutes(
    glz::http_router &Router,
    std::shared_ptr<db::Database> &Database
) -> std::expected<void, core::Error>;
```

With a pool, those should become:

```cpp
void registerCoreRoutes(
    glz::http_router &Router,
    std::shared_ptr<db::DatabasePool> Pool
);

auto registerRoutes(
    glz::http_router &Router,
    std::shared_ptr<db::DatabasePool> Pool
) -> std::expected<void, core::Error>;
```

And a handler becomes:

```cpp
Router.get("/accounts/:id", [Pool](const glz::request &Request, glz::response &Response) {
    auto Borrowed = Pool->acquire();
    if (!Borrowed) {
        Response.status(500).json({{"error", Borrowed.error().Message}});
        return;
    }

    auto Id = Request.params.at("id");
    auto Entry = (*Borrowed)->get<github::models::Account>(Id);
    if (!Entry) {
        Response.status(500).json({{"error", Entry.error().Message}});
        return;
    }

    Response.status(200).json(...);
});
```

That keeps the route style familiar:

- acquire at the top
- use the borrowed connection exactly like today
- automatic return at scope exit

---

## Pool Behavior

### Pool Size

Start with a small fixed size.

Reasonable initial choices:

- `4`
- `8`
- `min(8, hardware_concurrency())`

Do **not** blindly make the pool size equal to the worker thread count.
Database connections are expensive, and many request workloads do not need one
connection per CPU thread.

Good default:

```cpp
auto PoolSize = std::min<std::size_t>(8, std::max(2u, std::thread::hardware_concurrency()));
```

Later this should become config-driven.

### Empty Pool

When all connections are in use, `acquire()` should wait on a
`std::condition_variable` until one becomes available.

This is deliberate backpressure.

It is better for requests to queue briefly than to create unbounded new
connections or to keep sharing one connection unsafely.

### Reconnects

The existing `Database` object already contains reconnect logic inside
`withRetry()`.

Keep that logic in `Database`.

The pool should manage:

- ownership
- availability
- borrow/return

The `Database` object should keep managing:

- connection health
- reconnect attempts
- query execution

That separation keeps the pool small and focused.

---

## Health Endpoint

The current `/health` route directly uses `Database->Cx`, bypassing the safer
abstractions.

That should change.

Instead:

1. borrow a pooled connection
2. add a lightweight `ping()` or `healthcheck()` method on `Database`
3. have `/health` call that method

Example:

```cpp
std::expected<void, core::Error> ping() {
  return withRetry("Database::ping", [this]() -> void {
    pqxx::read_transaction Tx(Cx);
    Tx.exec("SELECT 1");
  });
}
```

This matters because the health route should follow the same safety and retry
path as the rest of the system.

---

## Recommended Rollout

### Phase 1: Add Pool Without Changing CRUD

Introduce:

- `DatabasePool`
- `PooledConnection`

Do **not** redesign `DbTraits<T>` or generic CRUD yet.

Goal:
- keep `Database::get/create/update/remove/getAll` as they are
- only change how route handlers obtain a `Database`

### Phase 2: Switch Core Routes

Update:

- `registerCoreRoutes`
- `/health`
- `/routes`

This is the smallest surface and validates the pool wiring.

### Phase 3: Switch GitHub Routes

Update:

- `include/insights/github/routes.hpp`
- `src/github/routes.cpp`
- startup wiring in `src/insights.cpp`

At this point, request-path DB access is safe.

### Phase 4: Add Pool Metrics

Optional but useful:

- active connections
- idle connections
- wait count
- max wait duration

These can later surface in logs or a debug endpoint.

---

## Files Likely To Change

New files:

- `include/insights/db/pool.hpp`
- `src/db/pool.cpp` or header-only implementation in `include/insights/db/`

Existing files:

- `include/insights/db/db.hpp`
- `include/insights/core/routes.hpp`
- `include/insights/github/routes.hpp`
- `src/core/routes.cpp`
- `src/github/routes.cpp`
- `src/insights.cpp`
- `CMakeLists.txt`
- `docs/architecture.md`

---

## Pseudocode Sketch

```cpp
class DatabasePool {
public:
  static std::expected<std::shared_ptr<DatabasePool>, core::Error>
  create(const std::string& ConnString, std::size_t PoolSize) {
    auto Pool = std::shared_ptr<DatabasePool>(new DatabasePool{});
    for (std::size_t I = 0; I < PoolSize; ++I) {
      auto Db = Database::connect(ConnString);
      if (!Db) {
        return std::unexpected(Db.error());
      }
      Pool->Available.push_back(*Db);
    }
    return Pool;
  }

  std::expected<PooledConnection, core::Error> acquire() {
    std::unique_lock Lock(Mutex);
    Cv.wait(Lock, [&] { return !Available.empty(); });

    auto Db = Available.front();
    Available.pop_front();
    return PooledConnection(this, std::move(Db));
  }

  void release(std::shared_ptr<Database> Db) {
    {
      std::lock_guard Lock(Mutex);
      Available.push_back(std::move(Db));
    }
    Cv.notify_one();
  }

private:
  std::mutex Mutex;
  std::condition_variable Cv;
  std::deque<std::shared_ptr<Database>> Available;
};
```

This is intentionally simple.

You do not need a lock-free structure here. Connection acquisition is already a
slow path relative to the actual query and network I/O.

---

## Design Choices

### Why Not Pool At The `pqxx::connection` Level Only

Pooling `Database` objects instead of raw `pqxx::connection`s keeps all current
retry logic and CRUD methods intact.

That minimizes refactor scope.

### Why Keep Background Tasks Separate For Now

The sync task is:

- infrequent
- long-running
- operationally different from short request/response handlers

Keeping it on a dedicated per-run connection avoids letting a long sync occupy a
slot in the HTTP pool for minutes.

### Why RAII Borrow Objects

Manual `acquire()` / `release()` pairs are fragile. Early returns in route
handlers are common.

RAII ensures:

- every successful acquire is released
- error paths stay short
- handlers remain readable

---

## Risks

### 1. Pool Starvation

If handlers borrow a connection and then do slow non-DB work before releasing
it, the pool can become exhausted.

Mitigation:

- keep DB borrows scoped as tightly as possible
- do network calls outside the borrow lifetime

### 2. Hidden Long Transactions

Some handlers may accidentally keep a borrowed connection alive across a larger
scope than intended.

Mitigation:

- prefer inner scopes:

```cpp
{
  auto Db = Pool->acquire();
  ...
} // returned here
```

### 3. Startup Failure If Pool Creation Partially Succeeds

If connection `5/8` fails during startup, pool construction should fail cleanly
and the process should exit rather than booting with a half-defined state.

Mitigation:

- `DatabasePool::create()` should be all-or-nothing

---

## Learning Summary

The core mistake in the current design is assuming that shared ownership implies
shared-thread safety.

It does not.

The right mental model is:

- `shared_ptr` answers "who owns this object?"
- a connection pool answers "who may use this object right now?"

For a multi-threaded server, you need both.

---

## Suggested Next Step

Implement the pool for HTTP routes only, with these constraints:

- keep background tasks on per-run connections
- add `Database::ping()` for `/health`
- change route registration to accept `std::shared_ptr<db::DatabasePool>`
- keep CRUD APIs on `Database` unchanged for the first pass

That gives you a safe, incremental refactor instead of a full DB layer rewrite.
