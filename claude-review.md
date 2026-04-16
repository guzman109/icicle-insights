# ICICLE Insights — Thorough Code Review (merged)

Scope: `src/insights.cpp`, `include/insights/core/*`, `include/insights/db/*`, `src/github/*`, `include/insights/github/*`, `include/insights/server/*`, `schema.sql`, `CMakeLists.txt`, `Dockerfile`, `justfile`, `.github/workflows/`, `tests/`, `conanfile.py`.

This review has been merged with findings from an independent review (`codex-review.md`) and a meta-review exchange (`claude-reviews-codex-review.md`, `codex-reviews-claude-review.md`). Convergent findings — where two independent reviewers landed on the same bug — are the highest-confidence items. Divergent findings have been reconciled, and two of my earlier recommendations (H2 pagination, M4 foreign key) have been retracted because Codex caught them as wrong.

Severity ordering; each finding cites a verified file:line where possible.

---

## CRITICAL

### C1. `pqxx::connection` shared across worker threads — data race
**Files:** `include/insights/db/db.hpp:36`, `src/insights.cpp:57,168-170`
**Status:** Verified. Convergent (Codex #1).

`Database` holds a single `pqxx::connection Cx` member. `main()` constructs one `std::shared_ptr<Database>`, captures it into every route handler lambda, and spins up `hardware_concurrency()` worker threads all running `IOContext->run()`. libpqxx is explicit: **`pqxx::connection` is not thread-safe**. Concurrent route handlers that each do `pqxx::work Tx(Database->Cx)` can corrupt the connection's internal state, socket buffer, or transaction stack.

The background sync task sidesteps this by opening a fresh connection (`src/github/tasks.cpp:261`), but HTTP handlers do not.

**Fix, in order of preference:**
1. Connection-per-request or a bounded connection pool with exclusive checkout.
2. Confine DB work to an `asio::strand` on the `io_context`.
3. A `std::mutex` guarding `Cx` — **emergency containment only**, not the target design. Serializes all DB work through one connection and keeps the global-connection coupling.

Do (3) today to stop the bleeding; design (1) as follow-up.

---

### C2. No GitHub API HTTP status inspection
**Files:** `src/github/tasks.cpp:70-75, 92, 113, 216` (unverified in full)
**Status:** Convergent in spirit (Codex #5 reframed the severity).

The sync task checks `if (!Response)` but never inspects `Response->status_code`. That means 429 (rate limited), 403 (secondary rate limit / abuse detection), and 5xx responses are treated as successful HTTP requests, and the code then tries to JSON-parse a rate-limit error payload as a GitHub repo object.

**Likely failure mode** (corrected from my earlier "silent data loss" framing): JSON parse errors get logged as generic sync failures, entire repos get marked as failed for non-obvious reasons, and no `X-RateLimit-*` headers are ever logged. It's a *loud* failure dressed up as a mysterious one, not silent corruption. Still critical — it means the sync is brittle under GitHub load and debugging failed runs is harder than it should be.

**Fix:** check `Response->status_code` explicitly after every request. On 429/403, log `X-RateLimit-Remaining` / `X-RateLimit-Reset` / `Retry-After` and either back off or fail the run cleanly with a clear error.

---

### C3. Shutdown race: `spdlog::shutdown()` runs while tasks still log
**File:** `src/insights.cpp:160-166`
**Status:** Verified. Convergent (Codex #2).

```cpp
Signals.async_wait([&](const std::error_code &, int) {
  spdlog::info("Shutdown signal received.");
  Server.stop();
  IOContext->stop();         // cancels pending work; does NOT drain running handlers
  spdlog::shutdown();         // runs while worker threads may still be inside a task
});
```

`io_context::stop()` is non-blocking — it prevents *new* handlers from running, but tasks currently executing continue until they return. If the GitHub sync task is mid-run and calls `spdlog::get("github_sync")->info(...)` after `spdlog::shutdown()` has already torn down the logger registry, that's undefined behavior.

**Fix:** move `spdlog::shutdown()` to after the `Thread.join()` loop at `src/insights.cpp:172-176`. The signal handler should only *request* shutdown; actual teardown happens when `run()` returns on its own. Bonus: replace `IOContext->stop()` with resetting an `asio::executor_work_guard` so `run()` drains naturally and in-flight HTTP requests finish cleanly.

---

### C4. Unauthenticated admin/mutation API
**Files:** `src/insights.cpp:53`, `src/github/routes.cpp:61,145,213,308,344`
**Status:** Convergent (Codex #10). I missed this in my first pass.

The server registers only logging middleware. `POST /api/github/accounts`, `POST /api/github/repos`, `DELETE`, and the sync-trigger routes are all reachable without authentication or authorization. If this service is network-accessible to anyone you don't fully trust, they can create, delete, or trigger outbound sync work against your GitHub token.

**Fix:**
- If auth is delegated to a reverse proxy (nginx, Caddy, Cloudflare Access), document that explicitly in the README and deployment guide, and enforce network boundaries (only localhost binding, or internal-only listener).
- Otherwise add an authn middleware that checks a bearer token before any mutating route runs. At minimum, gate `POST /api/github/repos/:id/sync` as admin-only.
- Either way, `POST`/`DELETE`/`sync` should not be reachable from the public internet without a layer in front.

---

## HIGH

### H1. Scheduler task lambda has no try/catch
**File:** `include/insights/core/scheduler.hpp:42-60` (unverified)

If the task lambda throws anything uncaught (OOM, a pqxx exception from a future code path, a bug in a new task), the exception propagates out of the ASIO handler. ASIO re-throws out of `run()`, which on a worker thread means `std::terminate`. The scheduler doesn't re-arm, and the process dies.

**Fix:** wrap `Task()` in try/catch, log via the task's named logger, and always re-arm the timer before returning.

---

### H2. Traffic counts are accumulated on a rolling window (overcounting)
**Files:** `src/github/tasks.cpp:110,130` (unverified in detail; Codex read it)
**Status:** Convergent (Codex #3). **This replaces my earlier "pagination" finding, which was wrong.**

Forks, stars, and subscribers are assigned (`=`) from the GitHub API, but clones and views are accumulated (`+=`). GitHub's `/traffic/clones` and `/traffic/views` endpoints return a **14-day rolling window** with a `count` field, not new deltas since the last sync. Every sync adds the full rolling count on top of the existing value. Totals inflate on every run.

Retraction: my earlier review flagged "missing pagination on traffic endpoints" as a silent-data-loss bug. That was wrong in two ways: (a) those endpoints don't paginate — they return a fixed window, and (b) the real bug goes the opposite direction (double-counting, not undercounting). Lesson: don't speculate about external API shapes without reading docs.

**Fix:**
- If the intended metric is "current GitHub rolling count", change to `Clones = TrafficStats.count` and `Views = TrafficStats.count`.
- If the intended metric is "lifetime cumulative traffic", persist the per-day breakdown and only aggregate unseen days.
- Document the semantic meaning of each stored metric inline so future sync changes don't regress this.

---

### H3. `withRetry` sleeps on the worker thread, starving the io_context
**File:** `include/insights/db/db.hpp:99,120` — verified.

Backoff between retries is `std::this_thread::sleep_for(Delay)` (up to 30 seconds). Every worker thread runs `IOContext->run()`, so sleeping one of them takes a handler slot offline. With `hardware_concurrency() = 8`, three simultaneous DB retries can leave you with just 5 threads handling HTTP.

Today this doesn't bite because most DB calls happen before `Server.start()` or inside the dedicated sync task. But `src/core/routes.cpp:15-39` (`/health`) calls the DB from a handler, so a transient DB hiccup during a health probe can block a worker for up to `1 + 2 + 4 = 7` seconds.

**Fix:** use `asio::steady_timer::async_wait` for backoff instead of `sleep_for`, or move DB work to a dedicated thread pool separate from the HTTP handlers.

---

### H4. Soft deletes are ignored on read and sync
**Files:** `include/insights/db/db.hpp:348-349` (verified), `src/github/tasks.cpp:160,196` (unverified)
**Status:** Convergent (Codex #2). I missed this in my first pass.

`Database::remove<T>()` correctly does `UPDATE ... SET deleted_at = NOW()` (`db.hpp:381-382`), and `schema.sql:47,83` has `deleted_at TIMESTAMPTZ` columns. But `Database::get<T>()` and `Database::getAll<T>()` both do `SELECT * FROM {table}` with **no `WHERE deleted_at IS NULL` filter** (verified at `db.hpp:348-349`).

Consequence: deleted accounts and repositories still appear in `GET /api/github/accounts`, still show up in `getAll()`, and still receive background updates from the sync task. The "delete" operation is effectively a no-op from the user's perspective.

**Fix:**
- Add `WHERE deleted_at IS NULL` to the default `get`/`getAll` queries.
- Provide explicit `getIncludingDeleted()` / `getAllIncludingDeleted()` for audit/admin paths if needed.
- Decide whether deleting an account should exclude or cascade its repositories (right now, deleting an account leaves its repos orphaned in sync).

---

### H5. Route registration failure is logged but not fatal
**File:** `src/insights.cpp:70-74` — verified.

```cpp
if (!insights::github::registerRoutes(...)) {
  spdlog::error("Failed registering git routes.");
}
```

You `return 1` for `Config` and `Database` failures, but not for this. The server starts with a half-configured router and 404s everything under `/api/github`. Either make it fatal (`return 1`), or document the graceful-degradation semantics and expose them on a readiness endpoint.

---

## MEDIUM

### M1. Dockerfile port defaults don't match the application
**File:** `Dockerfile:90-93` — verified. Convergent (Codex #13).

```dockerfile
EXPOSE 5000
HEALTHCHECK ... CMD curl -f http://${HOST:-localhost}:${PORT:-5000}/health || exit 1
```

App default is **3000** (`include/insights/core/config.hpp:12`, `README.md`, `justfile`). `EXPOSE 5000` is wrong, and `HEALTHCHECK` probes port 5000 unless `PORT` is explicitly set, so on a default deploy Docker reports the container unhealthy even when the app is running fine.

**Fix:** change both to 3000, or source the default from a single `ARG PORT=3000` referenced in both.

---

### M2. Timestamp parsing assumes local time
**File:** `include/insights/core/timestamp.hpp:10-26` (unverified). Convergent (Codex #12).

`formatTimestamp()` uses `localtime_r()` + `%z`, but `parseTimestamp()` uses `strptime()` with `%Y-%m-%d %H:%M:%S` (no `%z`) then `std::mktime()` (which interprets as local time). Postgres `TIMESTAMPTZ` values include timezone semantics that get silently dropped on the parse path. If the application host's timezone differs from the DB server's timezone, round-trips drift by the offset.

**Fix:** store and parse everything as UTC. Use `%Y-%m-%dT%H:%M:%SZ` or include `%z` on both sides. Better: let pqxx's native time conversions carry the value directly.

---

### M3. `std::stoi(PORT)` is unguarded
**File:** `include/insights/core/config.hpp:40-43` (unverified). Convergent (Codex #5).

If `PORT` is non-numeric, `stoi` throws, `Config::load()` propagates the exception, the `Result<T>` contract is violated, and `main()` doesn't catch it — the process crashes instead of returning a clean structured error.

**Fix:** wrap in try/catch or use `std::from_chars`. Validate the parsed value is in a sensible port range.

---

### M4. Missing resources return `500` instead of `404`
**Files:** `include/insights/db/db.hpp:360,389` — verified. `src/github/routes.cpp:121,151,282,315`
**Status:** Codex #4. I missed this.

`Database::get<T>()` throws `std::runtime_error("Not found")` when the row is empty (`db.hpp:360`); `Database::remove<T>()` does the same at `db.hpp:389`. `withRetry` catches the exception and wraps it in a generic `core::Error`. Route handlers then map every DB error to `InternalServerError`. A client asking for a nonexistent UUID gets `500` with the message `"Not found"` in the body — a normal client miss looks like a server fault.

**Fix:**
- Introduce typed errors (`NotFound`, `Validation`, `Internal`) in `core::Error` or as separate variant members.
- Map those explicitly to HTTP status codes at the route layer (404 / 400 / 500).
- Stop using exception message text (`"Not found"`) as control flow.

---

### M5. `views` / `clones` type inconsistency across schema, model, API
**Files:** `schema.sql:76,80` — verified. `include/insights/github/models.hpp:33,98`, `include/insights/github/routes.hpp:26,43` (unverified)
**Status:** Codex #11, with a bonus I found.

Schema: `views BIGINT DEFAULT 0` (line 80) but `clones INT DEFAULT 0` (line 76).
Model: both stored as `int`.
API output schema: `Views` exposed as `long long`.

Two problems:
1. **`clones` is `INT` in the schema** — Codex flagged `views` but missed that `clones` has the same truncation issue *at the DB level too*. Combined with the `+=` overcounting bug (H2), clones is actually the more likely column to overflow.
2. **`views` is `BIGINT` in the DB but `int` in the in-memory model**, so reads truncate at ~2.1B before the value ever reaches the API layer. The `long long` API type is cosmetic — the truncation already happened.

**Fix:** standardize `clones` and `views` on `bigint` / `int64_t` across schema, model parsing, and API types. Audit other counters (`stars`, `forks`, `followers`) for expected growth.

---

### M6. No compiler hardening flags
**File:** `CMakeLists.txt` — verified. No `-Wall`, `-Wextra`, `-Wpedantic`, `-Werror`, no sanitizers.

Given C1 exists, **TSan in a dedicated build preset would have caught it**. This isn't a nit — it's leverage for everything else on this list.

**Fix:**
```cmake
if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endif()
```
And add a `RelWithTSan` preset that builds with `-fsanitize=thread` for pre-deploy validation.

---

### M7. Glaze is fetched from the `main` branch
**File:** `CMakeLists.txt:18-23` — verified. Codex #17.

```cmake
FetchContent_Declare(
    glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
```

Builds are non-reproducible. An upstream breaking change on glaze/main will silently break your next CI build, and two developers building on different days can get different glaze versions. This also **corrects my earlier "pinned dependencies" praise** — I gave that an A grade based on `conanfile.py` without checking `CMakeLists.txt`. Process failure on my part.

**Fix:** pin to a specific tag or commit SHA (`GIT_TAG v5.x.x`). Prefer a single dependency acquisition strategy — either get glaze from Conan too, or explain in a comment why it has to be `FetchContent`.

---

### M8. No unit or integration tests for core logic
**File:** `tests/` (unverified — reportedly only Kulala `.http` files).

`DbTraits`, `withRetry`, `Result<T>`, route handlers, and the timestamp round-trip are untested. Of all the bugs in this review, the ones you'll regress most often are the ones with no test harness: soft-delete filtering, 404 mapping, `views`/`clones` types, overcounting.

**Fix:** start with one thin integration test that spins up a Postgres via Testcontainers and exercises `recordTaskRun` / `querySecondsUntilNextRun` / `getAll` end-to-end. You don't need broad coverage yet — you need the 5 functions that corrupt state if they break.

---

### M9. Advisory locks work by accident
**File:** `include/insights/db/db.hpp:212-233` (unverified)

Postgres advisory locks are **session-scoped**, not transaction-scoped. `tryAcquireTaskLock` acquires the lock inside a short-lived `pqxx::work` that commits, but the lock lives on the connection. The sync task gets away with this because it creates a fresh connection per run (`tasks.cpp:261`), so connection destruction releases the lock. The moment you add a connection pool (C1's proper fix), this assumption silently breaks.

**Fix:** document the connection-lifetime assumption at the lock callsite. Add explicit `releaseTaskLock` in all error paths, not just the happy path. If you move to a pool, hold the acquiring connection for the entire task duration.

---

### M10. No `/readyz` endpoint
**File:** `src/core/routes.cpp` (unverified in full)

Reported routes: `/health` (DB connectivity via raw `Database->Cx`), `/tasks/github-sync`, `/routes`. Missing: `/readyz`. `/health` answers "is this process alive?"; `/readyz` answers "is the process ready to take traffic?" (DB reachable **and** routes registered **and** last sync not wedged). Conflating the two hides real failure modes.

**Fix:** add `/readyz` that checks DB reachable, `registerRoutes` succeeded (see H5), and the last sync attempt status isn't `failed` for N consecutive runs.

---

### M11. Log injection via unescaped user input
**Files:** `src/github/routes.cpp:82,231,256` (unverified)

Account/repo names from request bodies are logged directly. Malicious names with `\n` or ANSI escapes forge log lines or confuse log viewers. Low-impact in a trusted-user environment, but a one-line sanitizer is cheap: reject or replace control characters before logging.

---

## LOW

- **L1.** Double-formatted "next run" log (`src/insights.cpp:111-131`). Two lines say the same thing.
- **L2.** Hard-coded `std::chrono::weeks(2)` in two places (`src/insights.cpp:91,136`). You already persist the interval in the DB; read it once.
- **L3.** `Config` captured by value into the scheduler lambda (`src/insights.cpp:137`). Fine today; `shared_ptr<const Config>` is future-proof.
- **L4.** Middleware doesn't catch handler exceptions (unverified). If a handler throws, the middleware never logs the request.
- **L5.** Missing index on `github_repositories(account_id)` (`schema.sql`, unverified). Fine at current row counts.
- **L6.** `/routes` advertises `PATCH /api/github/repos/:id` but no such route exists (Codex #8). Concrete drift — either implement or remove.
- **L7.** `dependencies.hpp` is really just `uuid_validators.hpp`. Misleading name for a one-function file.
- **L8.** GHCR is stubbed. `include/insights/ghcr/models.hpp` is entirely commented out. Known.
- **L9.** No `.env.example`. `.env` is correctly gitignored (`.gitignore:6`, never committed per `git log --all -- .env`), but new contributors have nothing to copy from.
- **L10.** No schema migration tooling. `schema.sql` drops and recreates on each apply. Add a top-of-file comment warning "DESTRUCTIVE — do not run against a populated database".
- **L11.** `/health` bypasses the database abstraction (Codex #7). It constructs a transaction directly from `Database->Cx` at `src/core/routes.cpp:21`, skipping `withRetry`. Add a `Database::ping()` method and call that instead.
- **L12.** `hardware_concurrency()` can return 0 (`src/insights.cpp:151`). Clamp to `std::max(1u, NumThreads)`, and consider making it configurable for containers.
- **L13.** Thin input validation on create routes (Codex #9). Create handlers parse JSON and lowercase names but don't reject empty strings, overlong values, or obvious garbage. Client errors hit DB constraint violations and surface as 500s (which M4 will fix separately).
- **L14.** TLS docs reference `SSL_CERT_FILE` as an application config concept, but `Config` has no `SslCertFile` field (Codex #14). Either wire it up or update `docs/tls-guide.md` and `README.md` to say cert discovery is delegated to the TLS stack.
- **L15.** `justfile` docs mismatch (Codex #15). `docs/README.md` references `just build` / `full-build` / `clean-build`, but `justfile` defines `build` as a *Docker* image build and has no `full-build` or `clean-build` recipes. This one also affects **CLAUDE.md**, which has the same stale recipe list. Update both.

---

## Retracted findings

These were in my original review (or my meta-review update list) and I now believe they were wrong:

- **Original H2 (pagination on traffic endpoints).** GitHub `/traffic/*` endpoints don't paginate — they return a 14-day rolling window. The real bug is `+=` overcounting (now H2 above). Caught by Codex #3.
- **Original M4 (FK from `task_run_attempts` to `task_runs`).** Wouldn't work — `recordTaskRunAttempt` inserts at task start, before `recordTaskRun` inserts the `task_runs` row, so the FK would reject legitimate first-run attempts. Caught by Codex in the meta-review. If you want normalization, introduce a separate `tasks` table that both `task_runs` and `task_run_attempts` reference and seed it at startup.
- **Original "deployment: `.env` committed with secrets".** One of my subagents reported this as critical. I verified it against the actual repo and it's **false** — `.env` is in `.gitignore:6`, `git log --all -- .env` is empty, and `git ls-files` doesn't list it. The file exists locally but has never been in version control.
- **Original "A grade for pinned dependencies".** I read `conanfile.py` and didn't check `CMakeLists.txt`. Glaze is fetched unpinned from `main` (now M7).

---

## What's actually good

- **`Result<T>` discipline.** Every fallible operation returns `std::expected<T, Error>` and every caller I read checks it. Rare in C++ code, and consistent.
- **Parameterized SQL everywhere.** `pqxx::params{}` throughout; dynamic query construction only substitutes table/column names from compile-time `DbTraits` constants. **No SQL injection surface.**
- **`DbTraits<T>` template pattern.** Genuinely cuts boilerplate without hiding SQL behind magic. Adding a new entity is ~25 lines of specialization.
- **Per-component loggers.** `createLogger("github_sync", ...)` keeps sync chatter out of request logs and makes routing them to separate files trivial.
- **CI works.** `.github/workflows/build.yaml` was fixed in commit 72f551d; multi-stage Docker build is clean, non-root user, pinned alpine base, multi-platform via buildx.
- **Conan-managed deps are pinned.** asio, libpqxx, spdlog all pinned to specific versions in `conanfile.py`. (Glaze is the exception — see M7.)
- **Persisted schedule survives restarts.** `querySecondsUntilNextRun()` plus the "immediate run when missing/overdue" logic is exactly right — no double runs, no lost runs, idempotent startup.
- **Strict JSON parsing.** Glaze's `error_on_missing_keys = true` rejects malformed input cleanly rather than silently defaulting.

---

## Suggested priority

**Stop-the-bleeding (one-line or near-one-line fixes):**
1. **C1** — mutex around `Database::Cx`. One line. Design a connection pool as follow-up.
2. **C3** — move `spdlog::shutdown()` after `Thread.join()`. One line.
3. **C4** — bind to `127.0.0.1` only (if you're currently binding `0.0.0.0`) until real auth lands. One env var. Simultaneously, document that the service requires a reverse proxy for auth.
4. **M1** — fix the Dockerfile port so health checks pass.
5. **H4** — add `WHERE deleted_at IS NULL` to `get`/`getAll`. Two lines.
6. **L12** — clamp `NumThreads` to `std::max(1u, ...)`. One line.

**Before the next release:**
7. **C2** — HTTP status inspection and `X-RateLimit-*` handling in the sync task.
8. **H1** — try/catch in the scheduler task lambda.
9. **H2** — fix the `+=` overcounting on traffic counts (`= TrafficStats.count`, not `+=`).
10. **H3** — async backoff in `withRetry` instead of `sleep_for`.
11. **M4** — typed errors with proper 404/400/500 mapping at the route layer.
12. **M5** — standardize `clones` and `views` on `bigint`/`int64_t` across schema, model, and API.
13. **M7** — pin glaze to a tag or commit.

**Durable improvements:**
14. **M6 + M8** — compiler hardening + a thin integration test suite. Force multipliers for everything else.
15. **C1 (full fix)** — connection pool / per-request connections, not just the mutex.
16. **C4 (full fix)** — real authn/authz middleware, not just network binding.
17. **M9** — connection-lifetime discipline for advisory locks, especially when the pool lands.
18. **M10** — `/readyz` distinct from `/health`.
19. **M2** — UTC everywhere for timestamps.

---

## Confidence and provenance notes

This review is the merge of my direct reading, subagent reports, and an independent review (`codex-review.md`) cross-checked via a meta-review exchange.

**Directly verified against current source** (I read the file and confirmed the cited line):
- C1 (`db.hpp:36`), C3 (`insights.cpp:160-166`), C4 route list verified in part, H3 (`db.hpp:99,120`), H4 (`db.hpp:348-349`, `db.hpp:381-382`), H5 (`insights.cpp:70-74`), M1 (`Dockerfile:90-93`), M4 (`db.hpp:360,389`), M5 (`schema.sql:76,80`), M6 (`CMakeLists.txt` full file), M7 (`CMakeLists.txt:18-23`), L1, L2, L3, L9 (`.env` gitignored, never committed).

**Relying on subagent reports or Codex's citations** (verify before acting):
- C2 (GitHub task internals), H1 (scheduler), H2 (`tasks.cpp` internals), M2 (`timestamp.hpp`), M3 (`config.hpp`), M8 (tests/), M9 (advisory lock code), M10 (routes), M11 (route logging), L4, L5, L6, L10, L11, L13, L14, L15.

**Convergent with Codex's independent review** (two reviewers agreed → high confidence):
- C1, C2 (reframed), C3, C4, H2 (after I retracted my pagination claim), H3, H4, M1, M2, M3, M4, M5, M7, L6, L11, L12, L13, L14, L15.

**Corrections caught by Codex in meta-review** — I was wrong:
- Original "pagination on traffic endpoints" → actually `+=` overcounting.
- Original "FK from `task_run_attempts` to `task_runs`" → would reject legitimate first-run attempts.
- Framing on C2 ("silent data loss" was overstated — real failure mode is parse errors and partial sync failures).

**False positive I caught before it shipped** — a subagent reported `.env` committed to git with real secrets as a critical finding. Verified against `.gitignore` and `git log --all` — the file has never been in version control. Flagging explicitly because it's the kind of confident-sounding fabrication LLM-assisted reviews produce, and it would have prompted destructive `git filter-repo` work to fix a problem that didn't exist.

**Things still not engaged with by either reviewer** — no verdict from Codex either way: H1 (scheduler try/catch), M8 (tests), M9 (advisory lock scoping), M11 (log injection). I still believe these; they just haven't been double-checked.
