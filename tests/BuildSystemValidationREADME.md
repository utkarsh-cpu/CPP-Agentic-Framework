# Build System Validation Tests

This document describes the comprehensive build system validation tests implemented for the C++ Agentic Framework. These tests ensure that the build system works correctly across different configurations, platforms, and scenarios.

## Overview

The build system validation consists of three main components:

1. **BuildSystemValidationTests.cpp** - C++ unit tests that validate build system functionality
2. **BuildSystemIntegrationTests.cpp** - Integration tests that verify build system configuration
3. **validate_build_system.sh** - Shell script for comprehensive build system testing

## Test Categories

### 1. Basic Configuration Tests

These tests verify that the CMake build system can be configured correctly:

- **Basic CMake Configuration**: Tests that CMake can configure the project successfully
- **Build Type Configuration**: Validates different build types (Debug, Release, RelWithDebInfo, MinSizeRel)
- **Compiler Detection**: Ensures compilers are detected and configured properly
- **Target Configuration**: Verifies that all required targets are created

### 2. Sanitizer Configuration Tests

These tests validate the sanitizer integration:

- **AddressSanitizer Configuration**: Tests ASan + UBSan + LeakSan configuration
- **ThreadSanitizer Configuration**: Validates TSan configuration
- **Mutually Exclusive Sanitizers**: Ensures sanitizers cannot be enabled simultaneously
- **Sanitizer Flag Verification**: Checks that correct flags are applied to targets

### 3. Static Analysis Integration Tests

These tests verify static analysis tool integration:

- **clang-tidy Integration**: Tests clang-tidy configuration and execution
- **cppcheck Integration**: Validates cppcheck configuration
- **Static Analysis Target**: Verifies custom static analysis targets work
- **Warning Detection**: Tests that warnings are properly detected and reported

### 4. Cross-Platform Compatibility Tests

These tests ensure the build system works across different platforms:

- **Generator Support**: Tests different CMake generators (Make, Ninja)
- **Compiler Compatibility**: Validates GCC, Clang, and MSVC support
- **Platform-Specific Features**: Tests platform-specific configurations
- **Installation Compatibility**: Verifies installation works on different platforms

### 5. Warning and Error Detection Tests

These tests validate compiler warning configuration:

- **Warning Level Configuration**: Tests comprehensive warning flags
- **Warnings as Errors**: Validates that warnings can be treated as errors
- **Warning Detection**: Creates intentional warnings to test detection
- **Compiler-Specific Warnings**: Tests GCC, Clang, and MSVC specific warnings

### 6. Dependency Management Tests

These tests verify dependency handling:

- **FetchContent Integration**: Tests Google Test and Benchmark fetching
- **Threading Support**: Validates threading library detection
- **Package Configuration**: Tests CMake package configuration files
- **Installation Verification**: Checks that dependencies are properly installed

## Running the Tests

### C++ Unit Tests

The C++ tests are integrated into the main test suite:

```bash
# Build and run all tests
cmake --preset=default
cmake --build --preset=default
ctest --preset=default

# Run only build system tests
./build/default/run_tests --gtest_filter="BuildSystem*"
```

### Shell Script Validation

The comprehensive shell script can be run independently:

```bash
# Make script executable (if not already)
chmod +x tests/validate_build_system.sh

# Run all validation tests
./tests/validate_build_system.sh

# The script will output detailed results for each test
```

### Using Build Script

The main build script includes validation:

```bash
# Run full CI pipeline (includes validation)
./build.sh ci

# Run specific configurations
./build.sh sanitizers
./build.sh static-analysis
```

## Test Implementation Details

### BuildSystemValidationTests.cpp

This file contains C++ unit tests using Google Test framework:

```cpp
class BuildSystemValidationTest : public ::testing::Test {
    // Test fixture with helper methods for:
    // - Command execution and output capture
    // - CMake configuration and building
    // - File system operations
    // - Tool availability checking
};
```

Key test methods:
- `executeCommand()` - Runs shell commands and captures output
- `configureCMake()` - Configures CMake with specific options
- `buildProject()` - Builds the project in a test directory
- `isToolAvailable()` - Checks if build tools are available

### BuildSystemIntegrationTests.cpp

This file contains integration tests that verify build system configuration:

- Tests CMakeLists.txt content for required features
- Validates CMakePresets.json configuration
- Checks build script functionality
- Verifies compiler and tool configurations

### validate_build_system.sh

Comprehensive shell script that tests:

- Multiple build configurations
- Sanitizer combinations
- Static analysis integration
- Cross-platform compatibility
- Installation and packaging

## Test Environment Requirements

### Required Tools

- CMake 3.15 or later
- C++20 compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
- Make or Ninja build system

### Optional Tools (for full validation)

- clang-tidy (for static analysis tests)
- cppcheck (for additional static analysis)
- AddressSanitizer, ThreadSanitizer, UBSanitizer support

### Platform Support

The tests are designed to work on:
- Linux (GCC, Clang)
- macOS (Clang, Apple Clang)
- Windows (MSVC, MinGW)

## Expected Test Results

### Successful Test Run

When all tests pass, you should see output like:

```
[INFO] Starting build system validation
[SUCCESS] PASSED: Basic CMake Configuration
[SUCCESS] PASSED: Build Types
[SUCCESS] PASSED: Sanitizer Configurations
[SUCCESS] PASSED: Static Analysis Integration
...
[INFO] Tests passed: 12
[INFO] Tests failed: 0
[SUCCESS] All build system validation tests PASSED
```

### Common Issues and Solutions

1. **Missing Tools**
   ```
   [WARNING] clang-tidy not found
   [WARNING] cppcheck not found
   ```
   Solution: Install missing tools or tests will be skipped

2. **Sanitizer Issues on macOS**
   ```
   [WARNING] Skipping sanitizer tests on Apple Clang
   ```
   Solution: Use Homebrew Clang or tests will be skipped

3. **CMake Version Issues**
   ```
   [WARNING] CMake version 3.14 doesn't support presets
   ```
   Solution: Upgrade CMake to 3.19+ for preset support

## Integration with CI/CD

The build system validation tests are designed to integrate with CI/CD pipelines:

### GitHub Actions Example

```yaml
name: Build System Validation
on: [push, pull_request]

jobs:
  validate-build-system:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    - name: Install dependencies
      run: |
        sudo apt update
        sudo apt install cmake ninja-build clang-tidy cppcheck
    - name: Run build system validation
      run: ./tests/validate_build_system.sh
    - name: Run C++ validation tests
      run: |
        cmake --preset=default
        cmake --build --preset=default
        ctest --preset=default --gtest_filter="BuildSystem*"
```

## Maintenance and Updates

When updating the build system:

1. **Add New Tests**: Create tests for new build system features
2. **Update Validation**: Modify validation scripts for new configurations
3. **Platform Testing**: Test on all supported platforms
4. **Documentation**: Update this README with new test information

## Troubleshooting

### Test Failures

If tests fail, check:

1. **Tool Availability**: Ensure required tools are installed
2. **Permissions**: Check that scripts are executable
3. **Environment**: Verify environment variables are set correctly
4. **Platform**: Some tests may be platform-specific

### Debug Mode

Enable verbose output for debugging:

```bash
# For shell script
VERBOSE=1 ./tests/validate_build_system.sh

# For C++ tests
./build/default/run_tests --gtest_filter="BuildSystem*" --gtest_verbose
```

## Contributing

When adding new build system features:

1. Add corresponding validation tests
2. Update both C++ and shell script tests
3. Test on multiple platforms
4. Update documentation
5. Ensure CI/CD integration works

This comprehensive validation ensures that the build system remains robust and reliable across all supported configurations and platforms.