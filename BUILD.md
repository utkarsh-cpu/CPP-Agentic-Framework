# Build System Documentation

This document describes the modern CMake build system for the Agentic Framework, including sanitizer integration, static analysis tools, and development workflows.

## Requirements

- CMake 3.15 or later
- C++20 compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
- Ninja build system (recommended)

### Optional Tools

- `clang-tidy` for static analysis
- `cppcheck` for additional static analysis
- AddressSanitizer, ThreadSanitizer, UBSanitizer (GCC/Clang)

## Quick Start

### Using CMake Presets (Recommended)

```bash
# Configure and build with default settings
cmake --preset=default
cmake --build --preset=default

# Run tests
ctest --preset=default
```

### Using the Build Script

```bash
# Make the script executable (Unix-like systems)
chmod +x build.sh

# Configure, build, and test with default settings
./build.sh configure
./build.sh build
./build.sh test

# Or run everything at once
./build.sh ci
```

## Build Configurations

### Available Presets

| Preset | Description | Use Case |
|--------|-------------|----------|
| `default` | Release build with high warnings | Production builds |
| `debug` | Debug build with symbols | Development |
| `sanitizers` | Debug + AddressSanitizer + UBSanitizer | Memory error detection |
| `tsan` | Debug + ThreadSanitizer | Race condition detection |
| `static-analysis` | Build with clang-tidy and cppcheck | Code quality analysis |
| `ci` | Full CI build with all checks | Continuous integration |

### Build Options

Configure the build using CMake options:

```bash
cmake -DENABLE_SANITIZERS=ON -DENABLE_STATIC_ANALYSIS=ON -B build
```

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_SANITIZERS` | OFF | Enable AddressSanitizer and UBSanitizer |
| `ENABLE_THREAD_SANITIZER` | OFF | Enable ThreadSanitizer (exclusive with other sanitizers) |
| `ENABLE_STATIC_ANALYSIS` | OFF | Enable clang-tidy and cppcheck |
| `TREAT_WARNINGS_AS_ERRORS` | ON | Treat compiler warnings as errors |
| `BUILD_SHARED_LIBS` | OFF | Build shared libraries instead of static |

## Development Workflow

### 1. Standard Development

```bash
# Configure for development
cmake --preset=debug

# Build
cmake --build --preset=debug

# Run tests
ctest --preset=debug --output-on-failure
```

### 2. Memory Error Detection

```bash
# Build with sanitizers
cmake --preset=sanitizers
cmake --build --preset=sanitizers

# Run tests (will detect memory leaks, buffer overflows, etc.)
ctest --preset=sanitizers
```

### 3. Race Condition Detection

```bash
# Build with ThreadSanitizer
cmake --preset=tsan
cmake --build --preset=tsan

# Run tests (will detect data races)
ctest --preset=tsan
```

### 4. Static Analysis

```bash
# Configure with static analysis
cmake --preset=static-analysis

# Build (will run clang-tidy and cppcheck during compilation)
cmake --build --preset=static-analysis

# Or run static analysis separately
cd build/static-analysis
make static_analysis
```

### 5. Full CI Pipeline

```bash
# Run complete CI pipeline
./build.sh ci
```

This will:
1. Clean all build directories
2. Run default build and tests
3. Run sanitizer tests
4. Run ThreadSanitizer tests
5. Run static analysis

## Compiler Warnings

The build system enables comprehensive warning levels:

### GCC/Clang Warnings
- `-Wall -Wextra -Wpedantic`
- `-Wconversion -Wsign-conversion`
- `-Wcast-align -Wcast-qual`
- `-Wformat=2 -Wshadow`
- `-Wold-style-cast -Woverloaded-virtual`
- And many more...

### MSVC Warnings
- `/W4` (high warning level)
- `/permissive-` (disable non-conforming code)
- `/Zc:__cplusplus` (correct __cplusplus macro)

## Sanitizers

### AddressSanitizer (ASan)
Detects:
- Buffer overflows
- Use-after-free
- Memory leaks
- Double-free errors

### ThreadSanitizer (TSan)
Detects:
- Data races
- Deadlocks
- Thread safety violations

**Note**: ThreadSanitizer cannot be used simultaneously with other sanitizers.

### UndefinedBehaviorSanitizer (UBSan)
Detects:
- Integer overflow
- Null pointer dereference
- Invalid casts
- Undefined behavior

## Static Analysis

### clang-tidy
Enabled checks:
- `bugprone-*` - Bug-prone code patterns
- `cert-*` - CERT secure coding guidelines
- `clang-analyzer-*` - Clang static analyzer
- `cppcoreguidelines-*` - C++ Core Guidelines
- `modernize-*` - Modern C++ features
- `performance-*` - Performance improvements
- `readability-*` - Code readability

### cppcheck
Additional static analysis with:
- All checks enabled
- C++20 standard compliance
- Verbose output
- Error exit codes for CI

## Troubleshooting

### Common Issues

1. **Missing dependencies**
   ```bash
   # Ubuntu/Debian
   sudo apt install cmake ninja-build clang-tidy cppcheck
   
   # macOS
   brew install cmake ninja clang-format cppcheck
   ```

2. **Sanitizer runtime errors**
   - Set environment variables for better output:
     ```bash
     export ASAN_OPTIONS="detect_leaks=1:abort_on_error=1"
     export TSAN_OPTIONS="abort_on_error=1"
     export UBSAN_OPTIONS="abort_on_error=1"
     ```

3. **Static analysis false positives**
   - Use inline suppressions in code:
     ```cpp
     // NOLINT(clang-diagnostic-unused-variable)
     // cppcheck-suppress unusedVariable
     ```

4. **Build performance**
   - Use Ninja instead of Make: `-G Ninja`
   - Enable parallel builds: `cmake --build . --parallel`
   - Use ccache if available

### Environment Variables

| Variable | Purpose | Example |
|----------|---------|---------|
| `ASAN_OPTIONS` | AddressSanitizer options | `detect_leaks=1:abort_on_error=1` |
| `TSAN_OPTIONS` | ThreadSanitizer options | `abort_on_error=1` |
| `UBSAN_OPTIONS` | UBSanitizer options | `abort_on_error=1` |

## Integration with IDEs

### Visual Studio Code
1. Install CMake Tools extension
2. Select preset from command palette: `CMake: Select Configure Preset`
3. Build with: `CMake: Build`

### CLion
1. Open project
2. Go to Settings → Build → CMake
3. Add profiles for different presets

### Visual Studio
1. Open folder in Visual Studio
2. CMake presets will be automatically detected
3. Select configuration from dropdown

## Continuous Integration

Example GitHub Actions workflow:

```yaml
name: CI
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    - name: Install dependencies
      run: |
        sudo apt update
        sudo apt install cmake ninja-build clang-tidy cppcheck
    - name: Run CI pipeline
      run: ./build.sh ci
```

This build system ensures high code quality, catches errors early, and provides a robust foundation for development and deployment.