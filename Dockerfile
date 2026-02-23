# =============================================================================
# 1. GitHub Workers build the binary.
# 2. Build the Docker Image and copy the prebuilt binary over.
# 3. Push Image to Pods. 
# =============================================================================
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y wget ca-certificates \
    && wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc \
    && echo "deb http://apt.llvm.org/bookworm/ llvm-toolchain-bookworm-21 main" >> /etc/apt/sources.list.d/llvm-21.list \
    && apt-get update && apt-get install -y \
    libssl3 \
    libpq5 \
    libc++1-21 \
    libc++abi1-21 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY dist/build/icicle-insights ./icicle-insights

# Set up non-root user for security
RUN useradd -m icicle && \
  mkdir -p /app/logs && \
  chown -R icicle:icicle /app
USER icicle

# Expose the default port (override with -e PORT=... at runtime)
EXPOSE 5000

# Health check — falls back to app default port if PORT is not set
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -f http://${HOST:-localhost}:${PORT:-5000}/health || exit 1

CMD ["./icicle-insights"]
