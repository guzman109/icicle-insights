# Meson Build System Guide

This guide covers migrating the ICICLE Insights project from CMake to Meson.

## Table of Contents

- [Why Meson?](#why-meson)
- [Prerequisites](#prerequisites)
- [Project Structure](#project-structure)
- [Configuration Files](#configuration-files)
  - [meson.build](#mesonbuild)
  - [conanfile.py](#conanfilepy)
  - [justfile](#justfile)
  - [.clangd](#clangd)
- [Building the Project](#building-the-project)
- [Common Tasks](#common-tasks)
- [Troubleshooting](#troubleshooting)

## Why Meson?

Meson offers several advantages over CMake:

- **Speed**: Faster configuration and build times
- **Simplicity**: Cleaner, more readable build files (Python-like syntax)
- **Built-in features**: Native testing, benchmarking, and cross-compilation support
- **Better dependency handling**: Clearer dependency resolution
- **Compile commands**: Generates `compile_commands.json` automatically

## Prerequisites

Install Meson:

```bash
# macOS
brew install meson

# Linux
pip install meson

# Or via your package manager
apt install meson        # Debian/Ubuntu
dnf install meson        # Fedora
```

You'll also need:
- Ninja (build backend)
- Conan 2.x
- C++23 compiler

## Project Structure

```
insights/
├── meson.build              # Main build file (replaces CMakeLists.txt)
├── meson.options            # Build options (optional)
├── conanfile.py             # Updated for Meson
├── justfile                 # Build command shortcuts
├── .clangd                  # LSP configuration
├── src/
│   ├── meson.build          # Source files build config
│   └── *.cpp
├── include/
│   └── *.h
└── build/
    ├── conan/               # Conan-generated files
    ├── release/             # Release build
    └── debug/               # Debug build
```

## Configuration Files

### meson.build

Create `meson.build` in the project root:

```meson
project('icicle-insights', 'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++23',
    'warning_level=3',
    'werror=false',
    'buildtype=release',
  ]
)

# Compiler flags
add_project_arguments(
  '-Wall',
  '-Wextra',
  '-Wpedantic',
  language: 'cpp'
)

# Dependencies from Conan
asio_dep = dependency('asio', version: '>=1.30.2')
openssl_dep = dependency('openssl', version: '>=3.0.0')
glaze_dep = dependency('glaze', version: '>=4.0.1')
libpqxx_dep = dependency('libpqxx', version: '>=7.9.2')
spdlog_dep = dependency('spdlog', version: '>=1.14.1')

# Include directories
inc_dirs = include_directories('include')

# Source files
src_files = files(
  'src/insights.cpp',
  'src/git/router.cpp',
  'src/server/server.cpp',
  # Add more source files as needed
)

# Executable
executable('icicle-insights',
  sources: src_files,
  include_directories: inc_dirs,
  dependencies: [
    asio_dep,
    openssl_dep,
    glaze_dep,
    libpqxx_dep,
    spdlog_dep,
  ],
  install: true
)

# Optional: subdirectories
# subdir('tests')
# subdir('benchmarks')
```

### Alternative: Modular meson.build

For larger projects, you can split into multiple `meson.build` files:

**Root `meson.build`:**
```meson
project('icicle-insights', 'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++23',
    'warning_level=3',
    'buildtype=release',
  ]
)

# Global compiler flags
add_project_arguments('-Wall', '-Wextra', '-Wpedantic', language: 'cpp')

# Dependencies (shared across subdirectories)
asio_dep = dependency('asio', version: '>=1.30.2')
openssl_dep = dependency('openssl', version: '>=3.0.0')
glaze_dep = dependency('glaze', version: '>=4.0.1')
libpqxx_dep = dependency('libpqxx', version: '>=7.9.2')
spdlog_dep = dependency('spdlog', version: '>=1.14.1')

# Include directories
inc_dirs = include_directories('include')

# Build subdirectories
subdir('src')
```

**`src/meson.build`:**
```meson
src_files = files(
  'insights.cpp',
  'git/router.cpp',
  'server/server.cpp',
)

executable('icicle-insights',
  sources: src_files,
  include_directories: inc_dirs,
  dependencies: [asio_dep, openssl_dep, glaze_dep, libpqxx_dep, spdlog_dep],
  install: true
)
```

### conanfile.py

Update your `conanfile.py` to generate Meson files:

```python
from conan import ConanFile
from conan.tools.meson import MesonToolchain, Meson

class IcicleInsightsConan(ConanFile):
    name = "icicle-insights"
    version = "0.1.0"

    # Settings
    settings = "os", "compiler", "build_type", "arch"

    # Build system
    generators = "PkgConfigDeps", "MesonToolchain"

    # Dependencies
    def requirements(self):
        self.requires("asio/1.30.2")
        self.requires("openssl/3.3.2")
        self.requires("glaze/4.0.1")
        self.requires("libpq/16.6")
        self.requires("libpqxx/7.9.2")
        self.requires("spdlog/1.14.1")

    def build_requirements(self):
        self.tool_requires("meson/[>=1.0.0]")
        self.tool_requires("ninja/[>=1.11.0]")

    def layout(self):
        self.folders.source = "."
        self.folders.build = "build"
        self.folders.generators = "build/conan"
```

**Key changes from CMake:**
- Use `MesonToolchain` instead of `CMakeToolchain`
- Use `PkgConfigDeps` generator (Meson uses pkg-config for dependency discovery)
- Set `generators = "PkgConfigDeps", "MesonToolchain"`

### justfile

```justfile
# https://just.systems

set dotenv-load
set export

export PATH := join(justfile_directory(), ".env", "bin") + ":" + env_var('PATH')

default:
    @just --list

test-env:
    echo "DATABASE_URL: $DATABASE_URL"
    echo "GITHUB_TOKEN: $GITHUB_TOKEN"
    echo "TAPIS_TOKEN: $TAPIS_TOKEN"

# Install dependencies with Conan
deps:
    conan install . --build=missing --output-folder=build/conan

# Setup Meson build (Release)
setup:
    meson setup build/release --buildtype=release --native-file=build/conan/conan_meson_native.ini

# Setup Meson build (Debug)
setup-debug:
    meson setup build/debug --buildtype=debug --native-file=build/conan/conan_meson_native.ini

# Build the project (Release)
build:
    meson compile -C build/release

# Build the project (Debug)
build-debug:
    meson compile -C build/debug

# Full build from scratch (Release)
full-build: deps setup build

# Full build from scratch (Debug)
full-build-debug: deps setup-debug build-debug

# Reconfigure (after meson.build changes)
reconfigure:
    meson setup --reconfigure build/release --buildtype=release --native-file=build/conan/conan_meson_native.ini

# Reconfigure debug build
reconfigure-debug:
    meson setup --reconfigure build/debug --buildtype=debug --native-file=build/conan/conan_meson_native.ini

# Clean and rebuild
clean-build:
    rm -rf build
    just full-build

# Clean only release build
clean-release:
    rm -rf build/release
    just setup build

# Clean only debug build
clean-debug:
    rm -rf build/debug
    just setup-debug build-debug

# Run the application (Release)
run:
    ./build/release/icicle-insights

# Run the application (Debug)
run-debug:
    ./build/debug/icicle-insights

# Run tests (Release)
test:
    meson test -C build/release

# Run tests (Debug)
test-debug:
    meson test -C build/debug --verbose

# Show build configuration
info:
    meson configure build/release

# Install the application
install:
    meson install -C build/release

# Format meson.build files
format:
    meson format -i meson.build
```

### .clangd

Update `.clangd` to point to Meson's compile commands:

```yaml
CompileFlags:
  Add:
    - "-std=c++23"
    - "-Wall"
    - "-Wextra"
    - "-Wpedantic"
  CompilationDatabase: build/release/

# Diagnostics are configured in .clang-tidy
Diagnostics:
  UnusedIncludes: Strict
  MissingIncludes: Strict

# Editor features
InlayHints:
  Enabled: true
  ParameterNames: true
  DeducedTypes: true
  BlockEnd: true

Hover:
  ShowAKA: true

# Index settings for better code navigation
Index:
  Background: Build
```

**Note**: Meson automatically generates `compile_commands.json` in the build directory, so this configuration works out of the box.

## Building the Project

### First-time setup

```bash
# 1. Install dependencies
just deps

# 2. Configure build
just setup

# 3. Build
just build

# Or do all at once
just full-build
```

### Development workflow

```bash
# Edit code...

# Rebuild (incremental)
just build

# Run
just run

# For debug builds
just build-debug
just run-debug
```

### After modifying meson.build

```bash
just reconfigure
just build
```

## Common Tasks

### Adding a new source file

Edit `meson.build`:

```meson
src_files = files(
  'src/insights.cpp',
  'src/git/router.cpp',
  'src/server/server.cpp',
  'src/new_file.cpp',  # Add this
)
```

Then:
```bash
just reconfigure
just build
```

### Adding a new dependency

1. Update `conanfile.py`:
```python
def requirements(self):
    self.requires("asio/1.30.2")
    # ... existing deps ...
    self.requires("new-library/1.0.0")  # Add this
```

2. Update `meson.build`:
```meson
new_lib_dep = dependency('new-library', version: '>=1.0.0')

executable('icicle-insights',
  # ...
  dependencies: [
    asio_dep,
    # ... existing deps ...
    new_lib_dep,  # Add this
  ],
)
```

3. Rebuild:
```bash
just clean-build
```

### Adding tests

Create `tests/meson.build`:

```meson
test_exe = executable('test_suite',
  sources: files('test_main.cpp', 'test_database.cpp'),
  include_directories: inc_dirs,
  dependencies: [asio_dep, libpqxx_dep, spdlog_dep],
)

test('unit tests', test_exe)
```

Update root `meson.build`:
```meson
# At the end
subdir('tests')
```

Run tests:
```bash
just test
```

### Build options

Create `meson.options`:

```meson
option('enable_tests', type: 'boolean', value: false,
  description: 'Build and run tests')

option('enable_benchmarks', type: 'boolean', value: false,
  description: 'Build benchmarks')
```

Use in `meson.build`:

```meson
if get_option('enable_tests')
  subdir('tests')
endif

if get_option('enable_benchmarks')
  subdir('benchmarks')
endif
```

Configure:
```bash
meson setup build/release -Denable_tests=true
```

### Cross-compilation

Meson has excellent cross-compilation support. Create a cross-file:

**`cross/linux-x64.ini`:**
```ini
[binaries]
c = 'x86_64-linux-gnu-gcc'
cpp = 'x86_64-linux-gnu-g++'
ar = 'x86_64-linux-gnu-ar'
strip = 'x86_64-linux-gnu-strip'

[host_machine]
system = 'linux'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
```

Build:
```bash
meson setup build/cross --cross-file=cross/linux-x64.ini
meson compile -C build/cross
```

## Troubleshooting

### Dependencies not found

**Problem**: `Dependency "asio" not found`

**Solution**: Make sure Conan generated pkg-config files:
```bash
ls build/conan/*.pc
just deps  # Regenerate if missing
```

### Compile commands not working

**Problem**: LSP can't find headers

**Solution**:
1. Ensure `compile_commands.json` exists:
```bash
ls build/release/compile_commands.json
```

2. Rebuild if missing:
```bash
just reconfigure
just build
```

3. Restart your LSP/editor

### Wrong C++ standard

**Problem**: Code uses C++23 features but compiler complains

**Solution**: Check Meson configuration:
```bash
meson configure build/release | grep cpp_std
```

Should show `c++23`. If not:
```bash
meson configure build/release -Dcpp_std=c++23
just build
```

### Clean build needed

When in doubt:
```bash
just clean-build
```

## Meson vs CMake Comparison

| Feature | CMake | Meson |
|---------|-------|-------|
| Configuration | `cmake -B build` | `meson setup build` |
| Build | `cmake --build build` | `meson compile -C build` |
| Test | `ctest --test-dir build` | `meson test -C build` |
| Install | `cmake --install build` | `meson install -C build` |
| Reconfigure | Re-run cmake | `meson --reconfigure build` |
| Syntax | CMake language | Python-like |
| Speed | Slower | Faster |
| compile_commands.json | Via `CMAKE_EXPORT_COMPILE_COMMANDS` | Automatic |

## Additional Resources

- [Meson Documentation](https://mesonbuild.com/Manual.html)
- [Meson with Conan](https://docs.conan.io/en/latest/reference/conanfile/tools/meson.html)
- [Meson Tutorial](https://mesonbuild.com/Tutorial.html)
- [Meson C++ Guide](https://mesonbuild.com/CPP.html)

## Migration Checklist

- [ ] Install Meson (`brew install meson`)
- [ ] Create `meson.build` file
- [ ] Update `conanfile.py` generators
- [ ] Update `justfile` commands
- [ ] Verify `.clangd` configuration
- [ ] Run `just clean-build`
- [ ] Test build with `just build`
- [ ] Test application with `just run`
- [ ] Update CI/CD pipelines (if any)
- [ ] Update documentation
- [ ] Archive `CMakeLists.txt` (optional: `mv CMakeLists.txt CMakeLists.txt.bak`)
