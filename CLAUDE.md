# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ICICLE Insights is a C++23 HTTP server that collects and stores metrics about ICICLE project components from various platforms (Git hosting services and container registries). It tracks repositories, accounts, and platform-level statistics like stars, forks, clones, views, and watchers.

The project references a components.json file containing ~500 ICICLE components across different research thrusts (AI4CI, FoundationAI, CI4AI, Software, and use-inspired domains like Digital Agriculture, Smart Foodsheds, and Animal Ecology).

## Build System

This project uses **CMake** with **Conan 2.x** for dependency management. Build commands are wrapped in a **justfile**.

### Prerequisites

- C++23 compiler (GCC 13+, Clang 17+, or Apple Clang 15+)
- [Conan 2.x](https://conan.io/)
- [CMake](https://cmake.org/) 3.25+
- [Ninja](https://ninja-build.org/) (build system)
- [just](https://just.systems/) (command runner)
- PostgreSQL (for runtime)

### Common Commands

```bash
just deps        # Install dependencies with Conan
just setup       # Configure CMake build
just build       # Build the project
just run         # Run the application

just full-build  # All of the above (deps + setup + build)
just clean-build # Clean and rebuild from scratch
```

### Environment Variables

Create a `.env` file (loaded automatically by just):
- `DATABASE_URL` - PostgreSQL connection string
- `GITHUB_TOKEN` - GitHub API token
- `TAPIS_TOKEN` - Tapis API token

## Dependencies

Managed via Conan (`conanfile.py`):

| Package | Purpose |
|---------|---------|
| **asio** | Async I/O, networking |
| **openssl** | TLS/SSL |
| **glaze** | JSON serialization, HTTP routing |
| **libpq** / **libpqxx** | PostgreSQL C/C++ clients |
| **spdlog** | Logging |

See [docs/dev-guide/build-system.md](docs/dev-guide/build-system.md) for details on adding dependencies.

## Architecture

See [docs/architecture.md](docs/architecture.md) for detailed architecture documentation.

### Module Structure

```
include/
├── core/           # Foundation utilities (Result<T>, config, HTTP status)
├── db/             # PostgreSQL database layer
├── git/            # Git platform models and routing
├── containers/     # Container registry models
└── server/         # HTTP server initialization

src/
├── insights.cpp    # Entry point
├── git/router.cpp  # Git platform route handlers
└── server/         # Server implementation
```

### Key Patterns

**Error Handling**: Uses C++23 `std::expected<T, Error>` via `insights::core::Result<T>`. No exceptions at API boundaries.

**Database**: Generic CRUD via templates + `DbTraits<T>` specializations. `shared_ptr<Database>` for shared access across route handlers.

**HTTP Routing**: glaze's `http_router` with lambda handlers capturing the database connection.

## Code Style

- C++23 standard
- Namespaced by module: `insights::<module>::<submodule>`
- Header-only for small utilities, separate .cpp for implementations
- `#pragma once` for header guards
- Minimal includes for faster compilation

### Naming Conventions (LLVM Style)

- **Types** (classes, structs, enums, typedefs): `PascalCase` (e.g., `Platform`, `HttpStatus`)
- **Variables**: `PascalCase` (e.g., `Database`, `Config`, `NewPlatform`)
- **Functions**: `lowerCamelCase` verb phrases (e.g., `registerPlatformRoutes()`, `initServer()`)
- **Enumerators**: `PascalCase` (e.g., `Ok`, `BadRequest`, `NotFound`)
- **Struct/Class members**: `PascalCase` (e.g., `.Id`, `.Name`, `.PlatformId`)
- **No cryptic abbreviations**: Use descriptive names (e.g., `JsonError` not `Ec`, `ErrorCode` not `Err`)

## Development Notes

- Entry point: `src/insights.cpp`
- Add new source files to `CMakeLists.txt` executable sources
- Run `just setup` after modifying `CMakeLists.txt`
