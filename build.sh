#!/bin/bash

# Agentic Framework Build Script
# This script provides easy access to different build configurations

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_usage() {
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  configure [preset]  Configure build with preset (default: default)"
    echo "  build [preset]      Build with preset (default: default)"
    echo "  test [preset]       Run tests with preset (default: default)"
    echo "  clean              Clean all build directories"
    echo "  sanitizers         Build and test with sanitizers"
    echo "  tsan               Build and test with ThreadSanitizer"
    echo "  static-analysis    Run static analysis tools"
    echo "  ci                 Run full CI pipeline"
    echo "  help               Show this help message"
    echo ""
    echo "Available presets:"
    echo "  default            Release build with high warnings"
    echo "  debug              Debug build"
    echo "  sanitizers         Debug build with AddressSanitizer and UBSanitizer"
    echo "  tsan               Debug build with ThreadSanitizer"
    echo "  static-analysis    Build with static analysis tools"
    echo "  ci                 Full CI build with all checks"
}

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

check_dependencies() {
    local missing_deps=()
    
    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    fi
    
    if ! command -v make &> /dev/null; then
        missing_deps+=("make")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing_deps[*]}"
        log_info "Please install the missing dependencies and try again"
        exit 1
    fi
}

configure_build() {
    local preset=${1:-default}
    log_info "Configuring build with preset: $preset"
    cmake --preset="$preset"
    log_success "Configuration completed"
}

build_project() {
    local preset=${1:-default}
    log_info "Building project with preset: $preset"
    cmake --build --preset="$preset"
    log_success "Build completed"
}

run_tests() {
    local preset=${1:-default}
    log_info "Running tests with preset: $preset"
    ctest --preset="$preset" --output-on-failure
    log_success "Tests completed"
}

clean_builds() {
    log_info "Cleaning all build directories"
    rm -rf build/
    log_success "Clean completed"
}

run_sanitizers() {
    log_info "Running build and tests with sanitizers"
    configure_build "sanitizers"
    build_project "sanitizers"
    run_tests "sanitizers"
    log_success "Sanitizer tests completed"
}

run_tsan() {
    log_info "Running build and tests with ThreadSanitizer"
    configure_build "tsan"
    build_project "tsan"
    run_tests "tsan"
    log_success "ThreadSanitizer tests completed"
}

run_static_analysis() {
    log_info "Running static analysis"
    configure_build "static-analysis"
    build_project "static-analysis"
    
    if [ -d "build/static-analysis" ]; then
        cd build/static-analysis
        make static_analysis || true
        cd ../..
    fi
    
    log_success "Static analysis completed"
}

run_ci() {
    log_info "Running full CI pipeline"
    
    # Clean start
    clean_builds
    
    # Run different configurations
    log_info "Step 1/4: Default build and tests"
    configure_build "default"
    build_project "default"
    run_tests "default"
    
    log_info "Step 2/4: Sanitizer tests"
    run_sanitizers
    
    log_info "Step 3/4: ThreadSanitizer tests"
    run_tsan
    
    log_info "Step 4/4: Static analysis"
    run_static_analysis
    
    log_success "CI pipeline completed successfully"
}

# Main script logic
case "${1:-help}" in
    configure)
        check_dependencies
        configure_build "$2"
        ;;
    build)
        check_dependencies
        build_project "$2"
        ;;
    test)
        check_dependencies
        run_tests "$2"
        ;;
    clean)
        clean_builds
        ;;
    sanitizers)
        check_dependencies
        run_sanitizers
        ;;
    tsan)
        check_dependencies
        run_tsan
        ;;
    static-analysis)
        check_dependencies
        run_static_analysis
        ;;
    ci)
        check_dependencies
        run_ci
        ;;
    help|--help|-h)
        print_usage
        ;;
    *)
        log_error "Unknown command: $1"
        print_usage
        exit 1
        ;;
esac