# Codex Review Priorities

Date: 2026-04-15

## Priority Order

### P0: Fix before exposing the service to untrusted traffic

#### 1. Add authentication and authorization for admin endpoints

Why:

- The server currently exposes create, delete, and sync-trigger endpoints without any built-in auth layer.
- If the service is reachable beyond a tightly controlled internal boundary, anyone can mutate data or trigger outbound GitHub work.

Suggested work:

- Add an auth middleware or enforce a signed shared-secret/API-key layer.
- Restrict `POST`, `DELETE`, and manual sync routes to admin callers.
- If auth is delegated to infrastructure, document that clearly and lock down ingress accordingly.

Success criteria:

- Unauthenticated requests to mutating/admin routes are rejected.
- Deployment docs state exactly where auth is enforced.

#### 2. Replace the shared DB connection used by request handlers

Why:

- A single `pqxx::connection` is shared across multithreaded request handling.
- This is the most serious runtime correctness problem in the server implementation.

Suggested work:

- Introduce either per-request connections or a small connection pool with exclusive checkout.
- Remove direct route access to `Database::Cx`.
- Move `/health` to a DB method such as `ping()`.

Success criteria:

- No route handler shares one live connection concurrently.
- The DB abstraction owns connection lifecycle instead of route code.

## P1: Fix next to protect data correctness

#### 3. Make soft delete semantics real

Why:

- Deleted accounts and repositories still appear in reads and background sync.
- Current delete behavior is closer to “mark but continue using” than actual removal from active data.

Suggested work:

- Add `deleted_at IS NULL` filtering to default read paths.
- Add explicit “including deleted” methods only where needed.
- Decide how repository visibility should behave when its owning account is deleted.

Success criteria:

- Deleted rows disappear from normal API reads.
- Sync jobs skip deleted rows.

#### 4. Fix clone/view sync semantics

Why:

- Traffic counts are currently added on every sync even though GitHub returns rolling-window values.
- This causes permanent metric inflation.

Suggested work:

- Choose one metric definition:
- rolling current count, or
- cumulative count from per-day snapshots.
- Update the sync implementation and documentation to match that definition.

Success criteria:

- Re-running sync without new traffic does not inflate values.
- Metric meaning is documented.

#### 5. Normalize numeric widths for repository counters

Why:

- `views` is `BIGINT` in PostgreSQL, `int` in the model, and `long long` in output.
- That inconsistency can truncate large values and creates an unstable API contract.

Suggested work:

- Standardize `views` to `std::int64_t` or `long long` end-to-end.
- Audit `clones`, `followers`, and other counters for future width needs.

Success criteria:

- Schema, model, DB parsing, and JSON schemas use consistent numeric types.

## P2: Fix for API correctness and operability

#### 6. Introduce typed errors and correct HTTP mapping

Why:

- Not-found cases currently come back as `500`.
- Client mistakes and server faults are not distinguished cleanly.

Suggested work:

- Replace message-based control flow with typed error categories.
- Map `NotFound`, `Validation`, `Conflict`, and `Internal` explicitly in route handlers.

Success criteria:

- Missing IDs return `404`.
- Invalid payload semantics return `400` or `422`.
- Constraint conflicts can return `409` where appropriate.

#### 7. Align container defaults with app defaults

Why:

- The app defaults to port `3000`, while the container exposes and health-checks `5000`.
- This creates avoidable deployment confusion.

Suggested work:

- Pick one default port and use it consistently in:
- app config,
- Dockerfile,
- README,
- `justfile`,
- examples.

Success criteria:

- A default container run is reachable and healthy without extra env overrides.

#### 8. Make startup configuration validation robust

Why:

- Invalid `PORT` values can crash startup.
- Validation is thin and some client payload problems are deferred to the DB layer.

Suggested work:

- Harden env parsing with structured errors.
- Add route-level semantic validation for empty strings and malformed values.

Success criteria:

- Bad env values produce a clear startup error instead of a crash.
- Bad client payloads fail fast with client-error responses.

## P3: Fix for maintainability and predictability

#### 9. Clean up documentation drift

Why:

- `/routes` advertises an unimplemented `PATCH` route.
- TLS docs reference `Config->SslCertFile`, which does not exist.
- Developer docs and the `justfile` disagree on build commands.

Suggested work:

- Reconcile docs with the codebase.
- Remove stale examples and dead references.
- Consider generating route documentation from the registration source.

Success criteria:

- README, docs, and runtime behavior match.

#### 10. Improve timestamp handling

Why:

- Current parsing likely drops timezone information from `TIMESTAMPTZ`.
- That can distort operational timestamps and make logs/task state harder to interpret.

Suggested work:

- Use timezone-aware parsing or native pqxx conversions.
- Prefer UTC for stored and rendered operational timestamps.

Success criteria:

- DB timestamps round-trip without timezone ambiguity.

#### 11. Reduce scheduler and dependency risks

Why:

- Long sync work runs inline on the shared executor.
- `glaze` is fetched from `main`, making builds non-reproducible.
- `hardware_concurrency()` may return zero.

Suggested work:

- Clamp worker count to at least one.
- Consider isolating long-running sync work onto a dedicated executor.
- Pin `glaze` to a specific revision.

Success criteria:

- Startup is robust in constrained environments.
- Builds are reproducible.
- Sync work does not unpredictably degrade request handling.

## Recommended Execution Plan

### Phase 1

- Add auth around admin routes.
- Replace shared request DB connection usage.

### Phase 2

- Fix delete filtering.
- Fix traffic metric semantics.
- Normalize numeric widths for metrics.

### Phase 3

- Introduce typed errors and proper status mapping.
- Align container/runtime defaults.
- Harden config and payload validation.

### Phase 4

- Reconcile docs and examples.
- Fix timestamp handling.
- Pin dependencies and harden thread/scheduler behavior.

## Test Coverage To Add

- Request auth tests for admin endpoints.
- Integration tests for concurrent request DB access behavior.
- Delete behavior tests ensuring deleted rows do not appear in reads or sync.
- Sync tests verifying clones/views do not inflate across repeated runs.
- API tests for `404`, `400`, `409`, and `500` mapping.
- Startup tests for invalid env values.
- Container smoke test for default port and healthcheck behavior.
