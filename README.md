# ICICLE Insights

A high-performance C++23 HTTP server that collects and stores metrics about ICICLE project components from various platforms including Git hosting services (GitHub, GitLab) and container registries (DockerHub, Quay.io).

## Overview

ICICLE Insights tracks repositories, accounts, and platform-level statistics across the ICICLE research project ecosystem, monitoring metrics such as:

- **Git Platforms**: stars, forks, clones, views, watchers, followers
- **Container Registries**: pulls, stars

The project tracks ICICLE components across different research thrusts:
- AI4CI (AI for Cyberinfrastructure)
- FoundationAI
- CI4AI (Cyberinfrastructure for AI)
- Software
- Use-inspired domains (Digital Agriculture, Smart Foodsheds, Animal Ecology)

## Features

- **Modern C++23** implementation with `std::expected` for error handling
- **Async I/O** using asio for high performance
- **Type-safe database operations** with PostgreSQL via libpqxx
- **RESTful API** with JSON serialization using glaze
- **Generic CRUD operations** using templates and traits
- **Docker support** for easy deployment

## Prerequisites

- C++23 compiler (GCC 13+, Clang 17+, or Apple Clang 15+)
- [Conan 2.x](https://conan.io/) - Package manager
- [CMake](https://cmake.org/) 3.25+
- [Ninja](https://ninja-build.org/) - Build system
- [just](https://just.systems/) - Command runner
- PostgreSQL - Database

## Quick Start

### 1. Clone the repository

```bash
git clone https://github.com/guzman109/icicle-insights.git
cd icicle-insights
```

### 2. Set up environment variables

Create a `.env` file:

```bash
DATABASE_URL=postgresql://user:password@localhost:5432/icicle_insights
GITHUB_TOKEN=your_github_token
TAPIS_TOKEN=your_tapis_token
```

### 3. Build and run

```bash
# Install dependencies, configure, build, and run
just full-build
just run

# Or individually:
just deps    # Install dependencies with Conan
just setup   # Configure CMake
just build   # Build the project
just run     # Run the server
```

The server will start on `http://localhost:3000`.

## API Endpoints

### Platforms

- `GET /api/git/platforms` - Get all platforms
- `POST /api/git/platforms` - Create a platform
- `GET /api/git/platforms/:id` - Get platform by ID
- `PATCH /api/git/platforms/:id` - Update platform
- `DELETE /api/git/platforms/:id` - Delete platform

### Accounts

- `GET /api/git/accounts` - Get all accounts
- `POST /api/git/accounts` - Create an account
- `GET /api/git/accounts/:id` - Get account by ID
- `PATCH /api/git/accounts/:id` - Update account
- `DELETE /api/git/accounts/:id` - Delete account

### Repositories

- `GET /api/git/repos` - Get all repositories
- `POST /api/git/repos` - Create a repository
- `GET /api/git/repos/:id` - Get repository by ID
- `PATCH /api/git/repos/:id` - Update repository
- `DELETE /api/git/repos/:id` - Delete repository

## Development

### Project Structure

```
.
├── include/          # Header files
│   ├── core/         # Foundation utilities
│   ├── db/           # Database layer
│   ├── git/          # Git platform models and routing
│   ├── containers/   # Container registry models
│   └── server/       # HTTP server
├── src/              # Implementation files
│   ├── git/          # Git route handlers
│   ├── server/       # Server implementation
│   └── main.cpp      # Entry point
├── docs/             # Documentation
├── tests/            # Test files (HTTP client tests)
└── data/             # Component data and fixtures
```

### Common Commands

```bash
just build           # Build the project
just run             # Run the server
just clean-build     # Clean and rebuild from scratch
just test            # Run tests (if configured)
```

### Code Style

The project follows LLVM naming conventions:
- **Types**: `PascalCase` (e.g., `Platform`, `HttpStatus`)
- **Variables**: `PascalCase` (e.g., `Database`, `Config`)
- **Functions**: `lowerCamelCase` (e.g., `registerRoutes()`, `initServer()`)
- **Enumerators**: `PascalCase` (e.g., `Ok`, `BadRequest`)

Code formatting is enforced via `.clang-format` and static analysis via `.clang-tidy`.

## Documentation

- [Architecture Guide](docs/architecture.md) - Detailed architecture and design decisions
- [CLAUDE.md](CLAUDE.md) - Project instructions for AI assistants and build system details

## Dependencies

| Package | Version | Purpose |
|---------|---------|---------|
| asio | 1.36.0 | Async I/O and networking |
| openssl | 3.6.0 | TLS/SSL support |
| glaze | 7.0.0 | JSON serialization and HTTP routing |
| libpq | 17.7 | PostgreSQL C client |
| libpqxx | 7.10.5 | PostgreSQL C++ client |
| spdlog | 1.17.0 | Logging |

## Docker

Build and run with Docker:

```bash
docker build -t icicle-insights .
docker run -p 3000:3000 --env-file .env icicle-insights
```

## License

[Add your license here]

## Contributing

[Add contribution guidelines here]

## Acknowledgments

This project is part of the [ICICLE (Intelligent Cyberinfrastructure with Computational Learning in the Environment)](https://icicle.osu.edu/) initiative.
