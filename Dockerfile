# =============================================================================
# Stage 1: Builder (Alpine)
# Installs build dependencies and compiles the binary with Conan.
# =============================================================================
FROM alpine:3.23.3 AS builder

RUN apk add --no-cache \
  autoconf \
  automake \
  bash \
  ca-certificates \
  ccache \
  clang \
  cmake \
  curl \
  git \
  libc++-dev \
  libpq-dev \
  linux-headers \
  llvm-libunwind-dev \
  libtool \
  lld \
  make \
  musl-dev \
  ninja \
  openssl-dev \
  perl \
  pkgconf \
  py3-pip \
  python3 \
  tar \
  unzip \
  zip

ENV CC=clang
ENV CXX=clang++

# Install Conan and auto-detect the build profile from the environment
RUN pip3 install conan --break-system-packages \
  && conan profile detect --force

WORKDIR /src

COPY conanfile.py .
COPY conan-overlays/ conan-overlays/
COPY . .

# Conan packages live in the cache mount and must be visible to cmake, so both
# steps share a single RUN. The ccache mount accelerates incremental rebuilds
# when source files change but dependencies do not.
RUN --mount=type=cache,target=/root/.conan2/p \
    --mount=type=cache,target=/root/.cache/ccache \
  conan create conan-overlays/libpq \
  && conan install . --build=missing \
    -s compiler.cppstd=gnu23 \
    -s compiler.libcxx=libc++ \
    --output-folder=/conan \
  && cmake -B build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/conan/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  && cmake --build build

# =============================================================================
# Stage 2: Runtime (Alpine)
# Copies only the compiled binary — no compilers, no Conan, no source.
# =============================================================================
FROM alpine:3.23.3

RUN apk add --no-cache \
  ca-certificates \
  curl \
  libgcc \
  libc++ \
  libpq \
  llvm-libunwind \
  openssl

WORKDIR /app
COPY --from=builder /src/build/icicle-insights ./icicle-insights

RUN chmod +x ./icicle-insights \
  && addgroup -S icicle \
  && adduser -S -G icicle icicle \
  && mkdir -p /app/logs \
  && chown -R icicle:icicle /app
USER icicle

EXPOSE 5000

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -f http://${HOST:-localhost}:${PORT:-5000}/health || exit 1

CMD ["./icicle-insights"]
