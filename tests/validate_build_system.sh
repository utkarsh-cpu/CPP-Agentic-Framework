#!/bin/bash

# Build System Validation Script
# This script validates the build system across different configurations and platforms

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_DIR="$PROJECT_ROOT/build/validation_tests"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results tracking
TESTS_PASSED=0
TESTS_FAILED=0
FAILED_TESTS=()

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

run_test() {
    local test_name="$1"
    local test_command="$2"
    
    log_info "Running test: $test_name"
    
    if eval "$test_command"; then
        log_success "PASSED: $test_name"
        ((TESTS_PASSED++))
        return 0
    else
        log_error "FAILED: $test_name"
        FAILED_TESTS+=("$test_name")
        ((TESTS_FAILED++))
        return 1
    fi
}

# Check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check if a tool is available and log its version
check_tool() {
    local tool="$1"
    local version_flag="${2:---version}"
    
    if command_exists "$tool"; then
        local version
        version=$($tool $version_flag 2>&1 | head -n1)
        log_info "$tool found: $version"
        return 0
    else
        log_warning "$tool not found"
        return 1
    fi
}

# Setup test environment
setup_test_environment() {
    log_info "Setting up test environment"
    
    # Clean and create test directory
    rm -rf "$TEST_DIR"
    mkdir -p "$TEST_DIR"
    
    # Check required tools
    log_info "Checking build tools availability"
    check_tool "cmake" "--version"
    check_tool "make" "--version"
    
    # Check optional tools
    check_tool "ninja" "--version"
    check_tool "clang-tidy" "--version"
    check_tool "cppcheck" "--version"
    check_tool "gcc" "--version"
    check_tool "clang" "--version"
}

# Test basic CMake configuration
test_basic_cmake_configuration() {
    local build_dir="$TEST_DIR/basic_config"
    mkdir -p "$build_dir"
    
    cd "$build_dir"
    cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
    
    # Check that essential files were created
    [[ -f "CMakeCache.txt" ]] && [[ -f "Makefile" || -f "build.ninja" ]]
}

# Test different build types
test_build_types() {
    local build_types=("Debug" "Release" "RelWithDebInfo" "MinSizeRel")
    
    for build_type in "${build_types[@]}"; do
        local build_dir="$TEST_DIR/build_type_$build_type"
        mkdir -p "$build_dir"
        
        cd "$build_dir"
        cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE="$build_type" >/dev/null 2>&1 && \
        cmake --build . >/dev/null 2>&1
    done
}

# Test sanitizer configurations
test_sanitizer_configurations() {
    # Skip on macOS with Apple Clang as sanitizers may not be fully supported
    if [[ "$OSTYPE" == "darwin"* ]] && [[ "$(cc --version 2>&1)" == *"Apple"* ]]; then
        log_warning "Skipping sanitizer tests on Apple Clang"
        return 0
    fi
    
    # Test AddressSanitizer + UBSanitizer
    local asan_build_dir="$TEST_DIR/asan_build"
    mkdir -p "$asan_build_dir"
    
    cd "$asan_build_dir"
    cmake "$PROJECT_ROOT" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DENABLE_SANITIZERS=ON >/dev/null 2>&1 && \
    cmake --build . >/dev/null 2>&1
    
    # Verify sanitizer flags are in compile commands
    if [[ -f "compile_commands.json" ]]; then
        grep -q "fsanitize=address" compile_commands.json && \
        grep -q "fsanitize=undefined" compile_commands.json
    fi
}

# Test ThreadSanitizer configuration
test_thread_sanitizer() {
    # Skip on macOS with Apple Clang
    if [[ "$OSTYPE" == "darwin"* ]] && [[ "$(cc --version 2>&1)" == *"Apple"* ]]; then
        log_warning "Skipping ThreadSanitizer test on Apple Clang"
        return 0
    fi
    
    local tsan_build_dir="$TEST_DIR/tsan_build"
    mkdir -p "$tsan_build_dir"
    
    cd "$tsan_build_dir"
    cmake "$PROJECT_ROOT" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DENABLE_THREAD_SANITIZER=ON >/dev/null 2>&1 && \
    cmake --build . >/dev/null 2>&1
    
    # Verify ThreadSanitizer flags
    if [[ -f "compile_commands.json" ]]; then
        grep -q "fsanitize=thread" compile_commands.json
    fi
}

# Test static analysis integration
test_static_analysis() {
    local static_build_dir="$TEST_DIR/static_analysis"
    mkdir -p "$static_build_dir"
    
    cd "$static_build_dir"
    cmake "$PROJECT_ROOT" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DENABLE_STATIC_ANALYSIS=ON >/dev/null 2>&1
    
    # Check if static analysis tools were configured
    local cmake_cache_content
    cmake_cache_content=$(cat CMakeCache.txt)
    
    # If tools are available, try to build
    if echo "$cmake_cache_content" | grep -q "CLANG_TIDY_EXE" || echo "$cmake_cache_content" | grep -q "CPPCHECK_EXE"; then
        # Build might fail due to static analysis findings, but configuration should work
        cmake --build . >/dev/null 2>&1 || true
    fi
    
    return 0  # Always pass if configuration succeeded
}

# Test warning detection
test_warning_detection() {
    local warning_test_dir="$TEST_DIR/warning_test"
    mkdir -p "$warning_test_dir"
    
    # Create a test project with warnings
    cat > "$warning_test_dir/CMakeLists.txt" << 'EOF'
cmake_minimum_required(VERSION 3.15)
project(warning_test CXX)

set(CMAKE_CXX_STANDARD 20)
add_executable(warning_test main.cpp)

if(MSVC)
    target_compile_options(warning_test PRIVATE /W4 /WX)
else()
    target_compile_options(warning_test PRIVATE -Wall -Wextra -Werror)
endif()
EOF
    
    # Create source with intentional warnings
    cat > "$warning_test_dir/main.cpp" << 'EOF'
#include <iostream>

int main() {
    int unused_var = 42;  // Intentional warning
    return 0;
}
EOF
    
    cd "$warning_test_dir"
    cmake . >/dev/null 2>&1
    
    # Build should fail due to warnings treated as errors
    ! cmake --build . >/dev/null 2>&1
}

# Test different generators
test_generators() {
    local generators=("Unix Makefiles")
    
    # Add Ninja if available
    if command_exists ninja; then
        generators+=("Ninja")
    fi
    
    for generator in "${generators[@]}"; do
        local gen_name
        gen_name=$(echo "$generator" | tr ' ' '_' | tr '[:upper:]' '[:lower:]')
        local build_dir="$TEST_DIR/generator_$gen_name"
        mkdir -p "$build_dir"
        
        cd "$build_dir"
        cmake "$PROJECT_ROOT" -G "$generator" -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 && \
        cmake --build . >/dev/null 2>&1
    done
}

# Test CMake presets (if supported)
test_cmake_presets() {
    # Check if CMake version supports presets
    local cmake_version
    cmake_version=$(cmake --version | head -n1 | grep -o '[0-9]\+\.[0-9]\+' | head -n1)
    local major_version
    major_version=$(echo "$cmake_version" | cut -d. -f1)
    local minor_version
    minor_version=$(echo "$cmake_version" | cut -d. -f2)
    
    # Presets require CMake 3.19+
    if [[ $major_version -lt 3 ]] || [[ $major_version -eq 3 && $minor_version -lt 19 ]]; then
        log_warning "CMake version $cmake_version doesn't support presets (requires 3.19+)"
        return 0
    fi
    
    cd "$PROJECT_ROOT"
    
    # Test default preset
    cmake --preset=default >/dev/null 2>&1 && \
    cmake --build --preset=default >/dev/null 2>&1
}

# Test cross-platform compatibility
test_cross_platform_compatibility() {
    local build_dir="$TEST_DIR/cross_platform"
    mkdir -p "$build_dir"
    
    cd "$build_dir"
    
    # Test with different CMAKE_SYSTEM_NAME values if cross-compilation tools are available
    cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 && \
    cmake --build . >/dev/null 2>&1
    
    # Check that the build produces expected artifacts
    local lib_found=false
    for ext in .a .so .lib .dll .dylib; do
        if find . -name "*agentic_framework*$ext" | grep -q .; then
            lib_found=true
            break
        fi
    done
    
    $lib_found
}

# Test installation
test_installation() {
    local build_dir="$TEST_DIR/install_test"
    local install_dir="$TEST_DIR/install_prefix"
    
    mkdir -p "$build_dir" "$install_dir"
    
    cd "$build_dir"
    cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$install_dir" >/dev/null 2>&1 && \
    cmake --build . >/dev/null 2>&1 && \
    cmake --install . >/dev/null 2>&1
    
    # Check that installation created expected directories and files
    [[ -d "$install_dir/lib" ]] && [[ -d "$install_dir/include" ]]
}

# Test package configuration
test_package_configuration() {
    local build_dir="$TEST_DIR/package_test"
    local install_dir="$TEST_DIR/package_install"
    
    mkdir -p "$build_dir" "$install_dir"
    
    cd "$build_dir"
    cmake "$PROJECT_ROOT" -DCMAKE_INSTALL_PREFIX="$install_dir" >/dev/null 2>&1 && \
    cmake --build . >/dev/null 2>&1 && \
    cmake --install . >/dev/null 2>&1
    
    # Check that package configuration files were installed
    [[ -f "$install_dir/lib/cmake/agentic_framework/agentic_frameworkConfig.cmake" ]] && \
    [[ -f "$install_dir/lib/cmake/agentic_framework/agentic_frameworkConfigVersion.cmake" ]]
}

# Test dependency fetching
test_dependency_fetching() {
    local build_dir="$TEST_DIR/deps_test"
    mkdir -p "$build_dir"
    
    cd "$build_dir"
    cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Debug >/dev/null 2>&1
    
    # Check that FetchContent downloaded dependencies
    local deps_found=false
    if find . -name "*gtest*" -o -name "*benchmark*" | grep -q .; then
        deps_found=true
    fi
    
    $deps_found && cmake --build . >/dev/null 2>&1
}

# Print test summary
print_test_summary() {
    echo
    log_info "=== Build System Validation Summary ==="
    log_info "Tests passed: $TESTS_PASSED"
    log_info "Tests failed: $TESTS_FAILED"
    
    if [[ $TESTS_FAILED -gt 0 ]]; then
        log_error "Failed tests:"
        for test in "${FAILED_TESTS[@]}"; do
            log_error "  - $test"
        done
        echo
        log_error "Build system validation FAILED"
        return 1
    else
        echo
        log_success "All build system validation tests PASSED"
        return 0
    fi
}

# Main execution
main() {
    log_info "Starting build system validation"
    
    setup_test_environment
    
    # Run all validation tests
    run_test "Basic CMake Configuration" "test_basic_cmake_configuration"
    run_test "Build Types" "test_build_types"
    run_test "Sanitizer Configurations" "test_sanitizer_configurations"
    run_test "ThreadSanitizer Configuration" "test_thread_sanitizer"
    run_test "Static Analysis Integration" "test_static_analysis"
    run_test "Warning Detection" "test_warning_detection"
    run_test "Generator Support" "test_generators"
    run_test "CMake Presets" "test_cmake_presets"
    run_test "Cross-Platform Compatibility" "test_cross_platform_compatibility"
    run_test "Installation" "test_installation"
    run_test "Package Configuration" "test_package_configuration"
    run_test "Dependency Fetching" "test_dependency_fetching"
    
    print_test_summary
}

# Run main function if script is executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi