# https://just.systems
# CMake build system

CONAN_DEPS_DIR := "build/conan"
BUILD_DIR := "build"
OPENSSL_ROOT := if os() == "macos" { `brew --prefix openssl@3` } else { "/usr" }

set dotenv-load := true
set export := true

default:
    @just --list

deps:
    conan create conan-overlays/libpq
    conan install . --build=missing --output-folder={{ CONAN_DEPS_DIR }} -s compiler.cppstd=gnu23

# Configure CMake build
cmake-setup:
    cmake -B {{ BUILD_DIR }} \
          -DCMAKE_TOOLCHAIN_FILE={{ CONAN_DEPS_DIR }}/conan_toolchain.cmake \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_MAKE_PROGRAM=$(which ninja) \
          -G Ninja

# Build the project
cmake-build:
    cmake --build {{ BUILD_DIR }}

# Full build from scratch
cmake: deps cmake-setup cmake-build

# Clean and rebuild
cmake-clean:
    rm -rf build
    just cmake

# Run the application
local-run:
    {{ BUILD_DIR }}/icicle-insights

build:
    docker buildx build --platform linux/amd64 -t ghcr.io/icicle-ai/insights:latest .

run:
    docker run -itd -p 3000:3000 --env-file=.env --name insights ghcr.io/icicle-ai/insights:latest

start:
    docker start insights

stop:
    docker stop insights

rm:
    docker rm insights
