# Multi-stage Dockerfile for ICICLE Insights
# Builds a C++23 HTTP server with Conan dependencies

# =============================================================================
# Builder Stage - Compile the application
# =============================================================================
FROM ubuntu:24.04 AS builder

# Build arguments
ARG TARGETARCH

# Install build tools and dependencies
RUN apt-get update && apt-get install -y \
    clang-18 \
    clang-tools-18 \
    lld \
    cmake \
    ninja-build \
    python3-pip \
    libssl-dev \
    libpq-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Create symlinks for non-versioned compiler names
RUN ln -sf /usr/bin/clang-18 /usr/bin/clang && \
    ln -sf /usr/bin/clang++-18 /usr/bin/clang++

# Install Conan 2.x
RUN pip3 install --no-cache-dir conan --break-system-packages

# Set compiler environment
ENV CC=clang-18
ENV CXX=clang++-18

WORKDIR /app

# Copy dependency files first for better layer caching
COPY conanfile.txt CMakeLists.txt ./

# Detect Conan profile and install dependencies
# Use gnu23 to match the justfile configuration
RUN conan profile detect --force && \
    conan install . \
        --build=missing \
        --output-folder=build \
        -s compiler.cppstd=23

# Copy source code
COPY include/ ./include/
COPY src/ ./src/

# Build the application using Conan-generated presets
# The cmake_layout in conanfile.txt creates build/build/Release/
RUN cmake --preset conan-release && \
    cmake --build --preset conan-release

# =============================================================================
# Runtime Stage - Minimal production image
# =============================================================================
FROM debian:bookworm-slim

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libssl3 \
    libpq5 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the compiled binary from builder stage
COPY --from=builder /app/build/icicle-insights ./icicle-insights

# Copy data files (components.json, etc.)
COPY data/ ./data/

# Set up non-root user for security
RUN useradd -m insights && \
    mkdir -p /app/logs && \
    chown -R insights:insights /app
USER insights

# Expose the default port (override with -e PORT=... at runtime)
EXPOSE 5000

# Health check — falls back to app default port if PORT is not set
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://${HOST:-localhost}:${PORT:-5000}/health || exit 1

# Run the application
CMD ["./icicle-insights"]
