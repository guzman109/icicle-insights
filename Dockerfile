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
    libc++-18-dev \
    libc++abi-18-dev \
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
ENV CXXFLAGS="-stdlib=libc++"

WORKDIR /app

# Copy dependency files first for better layer caching
COPY conanfile.txt CMakeLists.txt ./

# Detect Conan profile and install dependencies
# Use gnu23 to match the justfile configuration
RUN conan profile detect --force && \
    conan install . \
        --build=missing \
        --output-folder=build \
        -s compiler.cppstd=gnu23 \
        -s compiler.libcxx=libc++

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
FROM ubuntu:24.04

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libc++1-18 \
    libc++abi1-18 \
    libssl3t64 \
    libpq5 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the compiled binary from builder stage
# Path matches Conan's cmake_layout: build/build/Release/
COPY --from=builder /app/build/build/Release/icicle-insights ./icicle-insights

# Copy data files (components.json, etc.)
COPY data/ ./data/

# Set up non-root user for security
RUN useradd -m -u 1000 insights && \
    chown -R insights:insights /app
USER insights

# Environment variables (override at runtime)
ENV DATABASE_URL=""
ENV GITHUB_TOKEN=""
ENV TAPIS_TOKEN=""
ENV PORT=8080

# Expose the default port
EXPOSE 8080

# Health check (adjust endpoint as needed)
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# Run the application
CMD ["./icicle-insights"]
