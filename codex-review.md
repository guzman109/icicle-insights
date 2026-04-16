# Codex Review

Date: 2026-04-15

## Findings

### 1. Shared `pqxx::connection` across request threads

Severity: high

`main()` creates a single `ServerDatabase` and passes it into all route handlers:

- [src/insights.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/insights.cpp:57)
- [src/insights.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/insights.cpp:66)
- [src/insights.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/insights.cpp:168)

That `Database` instance owns one mutable `pqxx::connection Cx`:

- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:35)

All CRUD methods open transactions on that same connection. Since the server runs multiple worker threads, concurrent requests can use the same PostgreSQL connection at the same time. `pqxx::connection` is not a safe shared request-level concurrency primitive. Under load, this can lead to transaction failures, undefined behavior, or request cross-talk.

### Suggestion

- Replace the single shared connection with either:
- one fresh connection per request/task, or
- a bounded connection pool with exclusive checkout per operation.
- Keep background tasks on separate connections from HTTP handlers.
- Avoid exposing `Database::Cx` directly to route code.

### 2. Soft deletes are not respected by reads or sync jobs

Severity: high

Delete handlers only set `deleted_at = NOW()`:

- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:381)

But the read methods still query all rows:

- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:348)
- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:438)

And the sync job iterates whatever `getAll()` returns:

- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:160)
- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:196)

In practice, deleted accounts and repositories still appear in API responses and still receive background updates.

### Suggestion

- Add `WHERE deleted_at IS NULL` to default `get()` and `getAll()` queries.
- If you need access to archived rows, add explicit methods such as `getIncludingDeleted()` or `getAllIncludingDeleted()`.
- Decide whether deleting an account should also exclude or cascade its repositories in application behavior.

### 3. Clone and view metrics are overcounted on every sync

Severity: medium

In `syncRepository()`, forks, stars, and subscribers are assigned from the GitHub API response:

- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:87)

But clones and views are accumulated with `+=`:

- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:110)
- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:130)

GitHub traffic endpoints return rolling-window counts, not new deltas since the previous sync. Adding them each run permanently inflates totals.

### Suggestion

- If the intended value is “current GitHub rolling count,” assign `Clones = TrafficStats.count` and `Views = TrafficStats.count`.
- If the intended value is “lifetime cumulative traffic,” persist the per-day breakdown and aggregate only unseen days.
- Document the semantic meaning of each stored metric in code or docs so future sync changes stay consistent.

### 4. Missing resources return `500` instead of `404`

Severity: medium

`Database::get()` and `remove()` throw `"Not found"`:

- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:354)
- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:388)

`withRetry()` converts that into a generic error:

- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:125)

The route handlers then map every DB error to `InternalServerError`:

- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:121)
- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:151)
- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:282)
- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:315)

That makes normal client misses look like server faults and weakens API correctness and observability.

### Suggestion

- Introduce typed errors such as `NotFound`, `Validation`, and `Internal`.
- Map those explicitly to HTTP status codes at the route layer.
- Avoid using exception message text as control flow.

## Additional Suggestions

- Add request-level tests covering not-found behavior, delete semantics, and sync metric updates.
- Add concurrency-focused integration testing once connection handling is changed.
- Consider moving the `/health` endpoint away from direct raw access to `Database->Cx` so it uses the same connection-management policy as the rest of the application.

## Additional Findings

### 5. Invalid `PORT` can crash startup

Severity: low

`Config::load()` parses `PORT` with `std::stoi` and does not catch parse errors:

- [include/insights/core/config.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/core/config.hpp:41)

If `PORT` is malformed, the process can terminate during startup instead of returning a structured configuration error.

### Suggestion

- Wrap numeric env parsing in validation and return `std::unexpected(Error{...})` on invalid input.
- Validate that the parsed port is in a sensible range.

### 6. Background task execution is blocking on the shared `io_context`

Severity: low

The scheduler runs task work inline inside the timer callback:

- [include/insights/core/scheduler.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/core/scheduler.hpp:42)
- [include/insights/core/scheduler.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/core/scheduler.hpp:51)

The GitHub sync path performs blocking HTTP and database work. Because it runs on the same `io_context` used by the server, long sync runs consume worker capacity that would otherwise serve requests.

There is also a sharper variant of this problem in the retry path: `withRetry()` uses `std::this_thread::sleep_for(...)` during reconnect backoff:

- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:99)
- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:120)

When database work is executed on a server worker thread, the retry path can block that thread for seconds at a time.

### Suggestion

- Move long-running sync work onto a dedicated worker thread or separate executor.
- Replace blocking retry sleeps with timer-based backoff if DB work continues to run on shared server workers.
- If you keep the shared executor model, bound the work carefully and monitor request latency during sync windows.

### 7. `/health` bypasses the database abstraction

Severity: low

The health endpoint constructs a transaction directly from `Database->Cx`:

- [src/core/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/core/routes.cpp:21)

That bypasses retry behavior and makes the endpoint depend on raw connection ownership details.

### Suggestion

- Add a lightweight `ping()` method on `Database` and use that from `/health`.
- Keep route handlers out of direct connection internals so future DB refactors are localized.

### 8. Route documentation does not match implemented routes

Severity: low

`/routes` advertises a `PATCH /api/github/repos/:id` endpoint:

- [src/core/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/core/routes.cpp:125)

I did not find a corresponding route implementation in the GitHub router:

- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:21)

That creates a docs/runtime mismatch for API consumers.

### Suggestion

- Either implement the route or remove it from `/routes` and README/docs.
- Consider generating route docs from the router registration source to avoid drift.

### 9. Input validation is still thin on create routes

Severity: low

The create handlers parse JSON and normalize names to lowercase, but there does not appear to be additional validation for empty names or obviously invalid payload values before hitting database constraints:

- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:74)
- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:226)

The current behavior pushes a number of client errors into generic DB failures.

### Suggestion

- Validate required strings for non-empty values and expected formatting before DB calls.
- Return `400` for invalid payload semantics instead of depending on downstream constraint failures.

### 10. Administrative API surface is unauthenticated

Severity: critical if reachable by untrusted callers

The server registers only logging middleware:

- [src/insights.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/insights.cpp:53)

There is no authentication or authorization middleware around routes that create, delete, or trigger sync operations:

- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:61)
- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:145)
- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:213)
- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:308)
- [src/github/routes.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/routes.cpp:344)

If this service is reachable by untrusted callers, anyone can mutate tracked entities or trigger outbound GitHub sync work.

### Suggestion

- Add an explicit authn/authz layer in the server if direct access is possible.
- If auth is intentionally delegated to a reverse proxy, make that assumption prominent in README and deployment docs and enforce network boundaries accordingly.
- Treat `POST /api/github/repos/:id/sync` as an admin-only endpoint.

### 11. `views` uses inconsistent integer types across schema, model, and API

Severity: medium

The database schema stores `views` as `BIGINT`:

- [schema.sql](/Users/guzman.109/Projects/ICICLE/insights/schema.sql:80)

But the in-memory repository model stores it as `int` and parses it from the row as `int`:

- [include/insights/github/models.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/github/models.hpp:33)
- [include/insights/github/models.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/github/models.hpp:98)

The create schema also uses `std::optional<int>`:

- [include/insights/github/routes.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/github/routes.hpp:26)

But the output schema exposes `Views` as `long long`:

- [include/insights/github/routes.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/github/routes.hpp:43)

That mismatch creates a real overflow/truncation risk once view counts exceed 32-bit range and makes the API contract inconsistent with the storage model.

### Suggestion

- Standardize `views` on `std::int64_t` or `long long` across schema, model, DB parsing, and response types.
- Audit other counters for future growth and choose widths intentionally.

### 12. Timestamp parsing likely drops timezone information

Severity: medium

Database timestamps are parsed with:

- [include/insights/core/timestamp.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/core/timestamp.hpp:13)
- [include/insights/core/timestamp.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/core/timestamp.hpp:14)

`parseTimestamp()` expects `%Y-%m-%d %H:%M:%S` and then feeds the result to `std::mktime`, which interprets the time as local time. PostgreSQL `TIMESTAMPTZ` values include timezone semantics, and formatted values commonly include offsets. That means timezone information can be ignored or misinterpreted when rows are converted into C++ time points.

### Suggestion

- Parse timestamps using a timezone-aware representation, or let pqxx/native time conversions carry the value directly if available.
- Use UTC consistently for stored and formatted operational timestamps unless there is a strong reason not to.

### 13. Container runtime defaults do not match application defaults

Severity: medium

The application default port is `3000`:

- [include/insights/core/config.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/core/config.hpp:12)

But the container exposes `5000` and its health check defaults to `5000`:

- [Dockerfile](/Users/guzman.109/Projects/ICICLE/insights/Dockerfile:90)
- [Dockerfile](/Users/guzman.109/Projects/ICICLE/insights/Dockerfile:93)

The local run recipe maps `3000:3000`:

- [justfile](/Users/guzman.109/Projects/ICICLE/insights/justfile:48)

This mismatch can make the image look unhealthy or unreachable unless `PORT` is overridden explicitly.

### Suggestion

- Align Docker `EXPOSE`, health check defaults, docs, and app defaults on one port.
- If the container should default to `5000`, set that in application config and examples too.

### 14. TLS/config documentation no longer matches the implementation

Severity: low

The docs and example env file advertise `SSL_CERT_FILE` as an application config concept:

- [README.md](/Users/guzman.109/Projects/ICICLE/insights/README.md:56)
- [example-env.txt](/Users/guzman.109/Projects/ICICLE/insights/example-env.txt:19)
- [docs/tls-guide.md](/Users/guzman.109/Projects/ICICLE/insights/docs/tls-guide.md:49)
- [docs/tls-guide.md](/Users/guzman.109/Projects/ICICLE/insights/docs/tls-guide.md:114)

But `Config` has no `SslCertFile` field:

- [include/insights/core/config.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/core/config.hpp:11)

And the current client path just calls `configure_system_ca_certificates()`:

- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:31)

The deployment may still work because underlying libraries can read environment variables directly, but the documented application contract is stale.

### Suggestion

- Either add explicit `SSL_CERT_FILE` handling to `Config` and client setup, or update the docs to state that certificate discovery is delegated to the underlying TLS stack.
- Remove example code that references nonexistent `Config->SslCertFile` unless that field is implemented.

### 15. Build and developer docs are out of sync with the actual `just` recipes

Severity: low

The developer guide says:

- `just build` compiles the project
- `just full-build` exists
- `just clean-build` exists

See:

- [docs/README.md](/Users/guzman.109/Projects/ICICLE/insights/docs/README.md:79)
- [docs/README.md](/Users/guzman.109/Projects/ICICLE/insights/docs/README.md:85)

But the actual `justfile` defines:

- `build` as a Docker image build
- no `full-build`
- no `clean-build`

See:

- [justfile](/Users/guzman.109/Projects/ICICLE/insights/justfile:44)
- [justfile](/Users/guzman.109/Projects/ICICLE/insights/justfile:33)
- [justfile](/Users/guzman.109/Projects/ICICLE/insights/justfile:36)

This increases onboarding friction and makes troubleshooting harder.

### Suggestion

- Update the docs to match the current recipes, or rename/add recipes to match the docs.
- Keep README, docs, and `justfile` aligned as part of any workflow changes.

### 16. `hardware_concurrency()` can yield zero worker threads

Severity: low

The server uses `std::thread::hardware_concurrency()` directly as the worker count:

- [src/insights.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/insights.cpp:151)

If that returns `0`, the code starts no worker threads and the server will not process work.

### Suggestion

- Clamp the worker count to at least `1`.
- Consider making the worker count configurable for debugging and container environments.

### 17. Dependency pinning is weak for `glaze`

Severity: low

`CMakeLists.txt` fetches `glaze` from the `main` branch:

- [CMakeLists.txt](/Users/guzman.109/Projects/ICICLE/insights/CMakeLists.txt:18)
- [CMakeLists.txt](/Users/guzman.109/Projects/ICICLE/insights/CMakeLists.txt:21)

That makes builds non-reproducible and can break unexpectedly when upstream changes land.

### Suggestion

- Pin `glaze` to a specific tag or commit hash.
- Prefer one dependency acquisition strategy where possible instead of mixing Conan and live `FetchContent` for core dependencies.

### 18. Shutdown tears down logging before worker threads finish

Severity: high

The signal handler stops the server, stops the `io_context`, and immediately calls `spdlog::shutdown()`:

- [src/insights.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/insights.cpp:160)

But worker threads are only joined afterward:

- [src/insights.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/insights.cpp:172)

`io_context::stop()` does not guarantee that currently executing handlers have finished. If in-flight work logs after `spdlog::shutdown()` tears down the logger registry, shutdown behavior becomes unsafe.

### Suggestion

- Move `spdlog::shutdown()` until after the thread join loop.
- Keep the signal handler limited to requesting shutdown rather than final teardown.

### 19. Scheduler callbacks are not exception-safe

Severity: medium

The recurring-task wrapper invokes `Task()` directly inside the timer handler:

- [include/insights/core/scheduler.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/core/scheduler.hpp:42)
- [include/insights/core/scheduler.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/core/scheduler.hpp:51)

There is no local `try/catch` around that invocation. If a task ever throws unexpectedly, the exception can unwind out of the ASIO handler and terminate the process or at minimum kill the recurring schedule.

### Suggestion

- Wrap scheduled task execution in `try/catch`.
- Log the failure and re-arm the timer predictably even after task exceptions.

### 20. GitHub sync does not inspect HTTP status codes or rate-limit headers

Severity: high

The GitHub sync code checks whether the HTTP request object itself failed, but it does not appear to branch on returned HTTP status codes before parsing the body:

- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:70)
- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:91)
- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:112)
- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:215)

That means API-level failures such as `403`, `404`, or `429` are not handled explicitly, and rate-limit metadata such as `Retry-After` or GitHub rate-limit headers are ignored. The likely outcome is partial sync failure and noisy parse errors instead of controlled backoff or classified task failure.

### Suggestion

- Check `status_code` explicitly on every GitHub API response.
- Handle rate-limit and abuse-detection responses separately from parse failures.
- Log rate-limit headers and stop or defer the run cleanly when limits are reached.

### 21. Advisory lock behavior depends on connection lifetime

Severity: medium

The sync task acquires a PostgreSQL advisory lock using the task's database connection:

- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:213)
- [include/insights/db/db.hpp](/Users/guzman.109/Projects/ICICLE/insights/include/insights/db/db.hpp:225)
- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:301)

This works with the current design because the sync job opens a fresh connection for the run and destroys it when the run ends:

- [src/github/tasks.cpp](/Users/guzman.109/Projects/ICICLE/insights/src/github/tasks.cpp:261)

Advisory locks are session-scoped. If the code later moves to pooled connections without revisiting this logic, lock ownership semantics will silently change and stale locks become a real risk.

### Suggestion

- Keep the explicit `releaseTaskLock()` pattern even if connection lifetime currently makes cleanup likely.
- Document that the lock strategy depends on per-run connection ownership.
- Revisit the lock design when introducing a connection pool.

## Build Verification

I attempted a local build, but verification was blocked because CMake tried to fetch `glaze` from GitHub and this environment does not currently have network access.
