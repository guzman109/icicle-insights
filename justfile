# https://just.systems
# CMake build system

set dotenv-load
set export

default:
    @just --list

# Install dependencies with Conan
deps:
    conan install . --build=missing --output-folder=build -s compiler.cppstd=gnu23

# Setup CMake build
setup:
    cmake --preset conan-release

# Build the project
build:
    cmake --build --preset conan-release

# Full build from scratch
full-build: deps setup build

# Clean and rebuild
clean-build:
    rm -rf build
    just full-build

# Run the application
run:
    ./build/build/Release/icicle-insights
