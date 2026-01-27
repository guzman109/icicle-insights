# Meson Quick Reference

A quick reference for common Meson commands and patterns for the ICICLE Insights project.

## Quick Start

```bash
# Full build from scratch
just full-build

# Or manually:
just deps          # Install dependencies
just setup         # Configure
just build         # Build
just run           # Run
```

## Common Commands

| Task | Command | Description |
|------|---------|-------------|
| **Setup** | `just setup` | Configure release build |
| | `just setup-debug` | Configure debug build |
| **Build** | `just build` | Incremental build (release) |
| | `just build-debug` | Incremental build (debug) |
| | `just full-build` | Clean + setup + build (release) |
| | `just clean-build` | Remove build/ and rebuild |
| **Run** | `just run` | Run release binary |
| | `just run-debug` | Run debug binary |
| **Test** | `just test` | Run tests (release) |
| | `just test-debug` | Run tests (debug, verbose) |
| **Clean** | `rm -rf build/release` | Clean release only |
| | `rm -rf build/debug` | Clean debug only |
| | `rm -rf build` | Clean everything |
| **Info** | `just info` | Show build configuration |
| | `meson introspect build/release --buildoptions` | List all options |
| **Reconfigure** | `just reconfigure` | Apply meson.build changes |

## Direct Meson Commands

If you prefer to use Meson directly instead of `just`:

```bash
# Setup
meson setup build/release --buildtype=release --native-file=build/conan/conan_meson_native.ini

# Build
meson compile -C build/release

# Run
./build/release/icicle-insights

# Test
meson test -C build/release

# Reconfigure
meson setup --reconfigure build/release

# Clean (Meson doesn't have a clean command, just delete the directory)
rm -rf build/release
```

## Build Types

| Build Type | Optimizations | Debug Info | Use Case |
|------------|--------------|------------|----------|
| `release` | `-O3` | Minimal | Production, benchmarks |
| `debug` | `-O0` | Full (`-g`) | Development, debugging |
| `debugoptimized` | `-O2` | Full (`-g`) | Testing with some speed |
| `minsize` | `-Os` | Minimal | Size-constrained environments |

Set build type:
```bash
meson setup build/custom --buildtype=debugoptimized
```

## Directory Structure

```
build/
├── conan/                          # Conan-generated files
│   ├── conan_meson_native.ini      # Meson native file
│   ├── *.pc                        # pkg-config files
│   └── ...
├── release/                        # Release build
│   ├── compile_commands.json       # For LSP/clangd
│   ├── build.ninja                 # Ninja build file
│   └── icicle-insights             # Binary
└── debug/                          # Debug build
    ├── compile_commands.json
    └── icicle-insights
```

## meson.build Patterns

### Define executable

```meson
executable('my-app',
  sources: files('src/main.cpp', 'src/foo.cpp'),
  include_directories: include_directories('include'),
  dependencies: [dep1, dep2],
  cpp_args: ['-DSOME_DEFINE'],
  install: true
)
```

### Find dependencies

```meson
# Via pkg-config (Conan provides these)
dep = dependency('library-name', version: '>=1.0.0')

# Fallback to subproject if not found
dep = dependency('library-name',
  version: '>=1.0.0',
  fallback: ['subproject-name', 'dep_var']
)
```

### Add subdirectory

```meson
# In root meson.build
subdir('src')
subdir('tests')

# In src/meson.build
executable('app', ...)

# In tests/meson.build
test_exe = executable('test_suite', ...)
test('unit tests', test_exe)
```

### Conditional compilation

```meson
if get_option('enable_feature')
  executable('app', ..., cpp_args: ['-DFEATURE_ENABLED'])
endif

if host_machine.system() == 'linux'
  # Linux-specific config
endif
```

### Configure file

```meson
conf_data = configuration_data()
conf_data.set('VERSION', meson.project_version())
conf_data.set('PREFIX', get_option('prefix'))

configure_file(
  input: 'config.h.in',
  output: 'config.h',
  configuration: conf_data
)
```

## Debugging

### Verbose build

```bash
meson compile -C build/release -v
```

### Show all compiler commands

```bash
meson compile -C build/release --verbose
```

### Check dependencies

```bash
meson introspect build/release --dependencies
```

### Check targets

```bash
meson introspect build/release --targets
```

### Dry run

```bash
meson compile -C build/release --dry-run
```

## Configuration Options

### View current configuration

```bash
meson configure build/release
```

### Change options

```bash
meson configure build/release -Dcpp_std=c++23
meson configure build/release -Dwarning_level=3
meson configure build/release -Dwerror=true
```

### Common options

| Option | Values | Description |
|--------|--------|-------------|
| `buildtype` | release, debug, debugoptimized, minsize | Build type |
| `cpp_std` | c++11, c++14, c++17, c++20, c++23 | C++ standard |
| `warning_level` | 0, 1, 2, 3 | Warning level |
| `werror` | true, false | Treat warnings as errors |
| `optimization` | 0, 1, 2, 3, s | Optimization level |
| `debug` | true, false | Include debug info |
| `b_lto` | true, false | Link-time optimization |
| `prefix` | /path | Install prefix |

## Environment Variables

Meson respects standard environment variables:

```bash
# Compiler selection
export CC=clang
export CXX=clang++

# Flags
export CXXFLAGS="-march=native"
export LDFLAGS="-L/custom/lib"

# Then setup
meson setup build/release
```

## Testing

### Run all tests

```bash
meson test -C build/release
```

### Run specific test

```bash
meson test -C build/release test_name
```

### Verbose output

```bash
meson test -C build/release --verbose
```

### With valgrind

```bash
meson test -C build/release --wrap='valgrind --leak-check=full'
```

### Benchmark mode

```bash
meson test -C build/release --benchmark
```

## Installation

### Install to system

```bash
meson install -C build/release
```

### Install to custom prefix

```bash
meson setup build/release --prefix=/usr/local
meson install -C build/release
```

### Staged install (for packaging)

```bash
DESTDIR=/tmp/staging meson install -C build/release
```

## Conan + Meson Integration

### Key files

| File | Purpose |
|------|---------|
| `conanfile.py` | Defines dependencies |
| `build/conan/conan_meson_native.ini` | Meson native file from Conan |
| `build/conan/*.pc` | pkg-config files for dependencies |

### Conan workflow

```bash
# 1. Install dependencies (generates Meson files)
conan install . --build=missing --output-folder=build/conan

# 2. Setup Meson (uses Conan-generated native file)
meson setup build/release \
  --native-file=build/conan/conan_meson_native.ini

# 3. Build
meson compile -C build/release
```

### If dependencies not found

Check that pkg-config files exist:

```bash
ls build/conan/*.pc
```

Check that Meson can find them:

```bash
PKG_CONFIG_PATH=build/conan meson introspect build/release --dependencies
```

## LSP Integration (clangd)

### Ensure compile_commands.json exists

```bash
ls build/release/compile_commands.json
```

Meson generates this automatically.

### .clangd configuration

```yaml
CompileFlags:
  CompilationDatabase: build/release/
```

### Reload LSP

After rebuilding:
- VS Code: Restart LSP server (Cmd/Ctrl+Shift+P → "Reload Window")
- Vim/Neovim: `:LspRestart`

## Troubleshooting

### "Dependency X not found"

```bash
# Regenerate Conan files
just deps

# Check pkg-config files exist
ls build/conan/*.pc

# Check PKG_CONFIG_PATH in native file
cat build/conan/conan_meson_native.ini
```

### "File meson.build not found"

Make sure you're in the project root:

```bash
ls meson.build  # Should exist
```

### Build fails after changing meson.build

```bash
just reconfigure
just build
```

### Stale build

```bash
just clean-build
```

### Wrong C++ standard

```bash
meson configure build/release -Dcpp_std=c++23
just build
```

## CMake → Meson Translation

| CMake | Meson |
|-------|-------|
| `add_executable(name src1.cpp src2.cpp)` | `executable('name', 'src1.cpp', 'src2.cpp')` |
| `target_include_directories(name PRIVATE dir)` | `include_directories('dir')` (pass to executable) |
| `target_link_libraries(name lib)` | `dependencies: [lib_dep]` in executable() |
| `find_package(Lib)` | `dependency('lib')` |
| `target_compile_definitions(name PRIVATE DEF)` | `cpp_args: ['-DDEF']` |
| `add_subdirectory(sub)` | `subdir('sub')` |
| `option(OPT "desc" OFF)` | In meson.options: `option('opt', type: 'boolean', value: false)` |
| `if(OPT)` | `if get_option('opt')` |
| `set(VAR value)` | `var = 'value'` |
| `configure_file(in out)` | `configure_file(input: 'in', output: 'out', ...)` |

## Resources

- [Meson Official Docs](https://mesonbuild.com/)
- [Full Migration Guide](./meson-guide.md)
- [Meson Tutorial](https://mesonbuild.com/Tutorial.html)
- [Conan + Meson Docs](https://docs.conan.io/2/reference/tools/meson.html)
