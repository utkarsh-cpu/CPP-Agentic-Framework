cmake_minimum_required(VERSION 3.15)
project(agentic_framework VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers (AddressSanitizer, UBSanitizer)" OFF)
option(ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer (mutually exclusive with other sanitizers)" OFF)
option(ENABLE_STATIC_ANALYSIS "Enable static analysis tools (clang-tidy, cppcheck)" OFF)
option(TREAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

# Create main library target
add_library(agentic_framework STATIC)

# Set target properties and features
target_compile_features(agentic_framework PUBLIC cxx_std_20)

# Compiler-specific warning flags and optimizations
if(MSVC)
    target_compile_options(agentic_framework PRIVATE
        /W4                     # High warning level
        /permissive-            # Disable non-conforming code
        /Zc:__cplusplus         # Enable correct __cplusplus macro
        /Zc:preprocessor        # Enable conforming preprocessor
    )
    if(TREAT_WARNINGS_AS_ERRORS)
        target_compile_options(agentic_framework PRIVATE /WX)
    endif()
else()
    # Common warnings for GCC and Clang
    target_compile_options(agentic_framework PRIVATE
        -Wall                   # Enable most warnings
        -Wextra                 # Enable extra warnings
        -Wpedantic              # Enable pedantic warnings
        -Wconversion            # Warn about type conversions
        -Wsign-conversion       # Warn about sign conversions
        -Wcast-align            # Warn about cast alignment
        -Wcast-qual             # Warn about cast qualifiers
        -Wformat=2              # Enable format string warnings
        -Wold-style-cast        # Warn about old-style casts
        -Woverloaded-virtual    # Warn about overloaded virtual functions
        -Wshadow                # Warn about variable shadowing
        -Wundef                 # Warn about undefined macros
        -Wunused                # Warn about unused variables
        -fdiagnostics-show-option # Show option names in diagnostics
    )
    
    # GCC-specific warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(agentic_framework PRIVATE
            -Wctor-dtor-privacy     # Warn about constructor/destructor privacy
            -Wdisabled-optimization # Warn when optimization passes are disabled
            -Winit-self             # Warn about uninitialized variables
            -Wlogical-op            # Warn about logical operations
            -Wmissing-include-dirs  # Warn about missing include directories
            -Wnoexcept              # Warn about noexcept violations
            -Wredundant-decls       # Warn about redundant declarations
            -Wsign-promo            # Warn about sign promotion
            -Wstrict-null-sentinel  # Warn about null sentinels
            -Wstrict-overflow=5     # Warn about strict overflow
        )
    endif()
    
    # Clang-specific warnings
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(agentic_framework PRIVATE
            -Wmost                  # Enable most warnings (Clang-specific)
            -Wextra-semi            # Warn about extra semicolons
            -Wcomma                 # Warn about comma operator misuse
            -Wconditional-uninitialized # Warn about conditionally uninitialized variables
        )
    endif()
    
    if(TREAT_WARNINGS_AS_ERRORS)
        target_compile_options(agentic_framework PRIVATE -Werror)
    endif()
endif()

# Sanitizer configuration
if(ENABLE_SANITIZERS AND ENABLE_THREAD_SANITIZER)
    message(FATAL_ERROR "Cannot enable both ENABLE_SANITIZERS and ENABLE_THREAD_SANITIZER simultaneously")
endif()

if(ENABLE_SANITIZERS)
    if(MSVC)
        message(WARNING "Sanitizers not fully supported on MSVC, skipping")
    else()
        set(SANITIZER_FLAGS
            -fsanitize=address          # AddressSanitizer for memory errors
            -fsanitize=undefined        # UndefinedBehaviorSanitizer
            -fsanitize=leak             # LeakSanitizer (part of AddressSanitizer)
            -fno-omit-frame-pointer     # Better stack traces
            -fno-optimize-sibling-calls # Better stack traces
        )
        target_compile_options(agentic_framework PRIVATE ${SANITIZER_FLAGS})
        target_link_options(agentic_framework PRIVATE ${SANITIZER_FLAGS})
        message(STATUS "Enabled AddressSanitizer, UndefinedBehaviorSanitizer, and LeakSanitizer")
    endif()
endif()

if(ENABLE_THREAD_SANITIZER)
    if(MSVC)
        message(WARNING "ThreadSanitizer not supported on MSVC, skipping")
    else()
        set(TSAN_FLAGS
            -fsanitize=thread           # ThreadSanitizer for race conditions
            -fno-omit-frame-pointer     # Better stack traces
        )
        target_compile_options(agentic_framework PRIVATE ${TSAN_FLAGS})
        target_link_options(agentic_framework PRIVATE ${TSAN_FLAGS})
        message(STATUS "Enabled ThreadSanitizer")
    endif()
endif()

# Static analysis tools integration
if(ENABLE_STATIC_ANALYSIS)
    # Find clang-tidy
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CLANG_TIDY_COMMAND
            ${CLANG_TIDY_EXE}
            -checks=-*,bugprone-*,cert-*,clang-analyzer-*,cppcoreguidelines-*,hicpp-*,misc-*,modernize-*,performance-*,portability-*,readability-*
            -warnings-as-errors=*
            -header-filter=.*
        )
        set_target_properties(agentic_framework PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}"
        )
        message(STATUS "Enabled clang-tidy static analysis")
    else()
        message(WARNING "clang-tidy not found, static analysis disabled")
    endif()

    # Find cppcheck
    find_program(CPPCHECK_EXE NAMES cppcheck)
    if(CPPCHECK_EXE)
        set(CPPCHECK_COMMAND
            ${CPPCHECK_EXE}
            --enable=all
            --std=c++20
            --verbose
            --quiet
            --error-exitcode=1
            --inline-suppr
            --suppress=missingIncludeSystem
            --suppress=unmatchedSuppression
            --suppress=unusedFunction
        )
        set_target_properties(agentic_framework PROPERTIES
            CXX_CPPCHECK "${CPPCHECK_COMMAND}"
        )
        message(STATUS "Enabled cppcheck static analysis")
    else()
        message(WARNING "cppcheck not found, static analysis disabled")
    endif()
endif()

# Add sources using target_sources for better organization
target_sources(agentic_framework PRIVATE
    BaseNode/BaseNode.cpp
    BaseNode/ConditionalTransition.cpp
    BaseNode/FrameworkError.cpp
    BaseNode/OptimizedBatchProcessor.cpp
    BaseNode/CancellationToken.cpp
    BaseNode/Logger.cpp
    BaseNode/PerformanceProfiler.cpp
    BaseNode/MemoryTracker.cpp
    BaseNode/AsyncTracer.cpp
    BaseNode/DiagnosticSystem.cpp
    Node/Node.cpp
    BatchNode/BatchNode.cpp
    Flow/Flow.cpp
    BatchFlow/BatchFlow.cpp
    AsyncNode/AsyncNode.cpp
    AsyncBatchNode/AsyncBatchNode.cpp
    AsyncParallelBatchNode/AsyncParallelBatchNode.cpp
    AsyncFlow/AsyncFlow.cpp
    AsyncBatchFlow/AsyncBatchFlow.cpp
    AsyncParallelBatchFlow/AsyncParallelBatchFlow.cpp
)

# Set public headers for installation
target_sources(agentic_framework PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        BaseNode/BaseNode.h
        BaseNode/ConditionalTransition.h
        BaseNode/FrameworkData.h
        BaseNode/FrameworkError.h
        BaseNode/Result.h
        BaseNode/ErrorHandling.h
        BaseNode/TypedData.h
        BaseNode/BatchData.h
        BaseNode/OptimizedBatchProcessor.h
        BaseNode/CancellationToken.h
        BaseNode/TimeoutPolicy.h
        BaseNode/AsyncTimeout.h
        BaseNode/InputValidation.h
        BaseNode/SecurityUtils.h
        BaseNode/Logger.h
        BaseNode/PerformanceProfiler.h
        BaseNode/MemoryTracker.h
        BaseNode/AsyncTracer.h
        BaseNode/DiagnosticSystem.h
        Flow/StringData.h
        BatchNode/VectorData.h
        BatchFlow/MapData.h
        Node/Node.h
        BatchNode/BatchNode.h
        Flow/Flow.h
        BatchFlow/BatchFlow.h
        AsyncNode/AsyncNode.h
        AsyncBatchNode/AsyncBatchNode.h
        AsyncParallelBatchNode/AsyncParallelBatchNode.h
        AsyncFlow/AsyncFlow.h
        AsyncBatchFlow/AsyncBatchFlow.h
        AsyncParallelBatchFlow/AsyncParallelBatchFlow.h
)

# Set include directories
target_include_directories(agentic_framework
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Add threading support for async components
find_package(Threads REQUIRED)
target_link_libraries(agentic_framework PUBLIC Threads::Threads)


# Set target properties
set_target_properties(agentic_framework PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    # Remove PUBLIC_HEADER line - it's redundant with FILE_SET HEADERS
)

# Create alias for better namespacing
add_library(agentic::framework ALIAS agentic_framework)

# Installation configuration
include(GNUInstallDirs)

install(TARGETS agentic_framework
    EXPORT agentic_frameworkTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/agentic_framework
)

# Install export file
install(EXPORT agentic_frameworkTargets
    FILE agentic_frameworkTargets.cmake
    NAMESPACE agentic::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/agentic_framework
)

# Create package configuration files
include(CMakePackageConfigHelpers)

# Generate the config file directly without template
set(CONFIG_FILE_CONTENT "
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)

# Import targets
if(NOT TARGET agentic::framework)
    include(\"\${CMAKE_CURRENT_LIST_DIR}/agentic_frameworkTargets.cmake\")
endif()

# Set variables for compatibility
set(agentic_framework_FOUND TRUE)
set(AGENTIC_FRAMEWORK_FOUND TRUE)

check_required_components(agentic_framework)
")

# Write the config content to a temporary .in file
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/agentic_frameworkConfig.cmake.in" "${CONFIG_FILE_CONTENT}")

configure_package_config_file(
    "${CMAKE_CURRENT_BINARY_DIR}/agentic_frameworkConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/agentic_frameworkConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/agentic_framework
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/agentic_frameworkConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/agentic_frameworkConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/agentic_frameworkConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/agentic_framework
)

# Google Test and Google Benchmark

enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.8.3
)

FetchContent_MakeAvailable(googletest googlebenchmark)

# Helper function to apply common settings to executables
function(configure_executable target_name)
    # Apply same warning levels as main library
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
        if(TREAT_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        # Common warnings for GCC and Clang
        target_compile_options(${target_name} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
            -Wcast-align -Wcast-qual -Wformat=2 -Wold-style-cast
            -Woverloaded-virtual -Wshadow -Wundef -Wunused
        )
        
        # GCC-specific warnings
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target_name} PRIVATE
                -Wlogical-op -Wnoexcept -Wstrict-null-sentinel
            )
        endif()
        
        # Clang-specific warnings
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target_name} PRIVATE
                -Wmost -Wextra-semi -Wcomma
            )
        endif()
        
        if(TREAT_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()

    # Apply sanitizer settings
    if(ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(${target_name} PRIVATE ${SANITIZER_FLAGS})
        target_link_options(${target_name} PRIVATE ${SANITIZER_FLAGS})
    endif()
    
    if(ENABLE_THREAD_SANITIZER AND NOT MSVC)
        target_compile_options(${target_name} PRIVATE ${TSAN_FLAGS})
        target_link_options(${target_name} PRIVATE ${TSAN_FLAGS})
    endif()

    # Apply static analysis settings
    if(ENABLE_STATIC_ANALYSIS)
        if(CLANG_TIDY_EXE)
            set_target_properties(${target_name} PROPERTIES
                CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}"
            )
        endif()
        if(CPPCHECK_EXE)
            set_target_properties(${target_name} PROPERTIES
                CXX_CPPCHECK "${CPPCHECK_COMMAND}"
            )
        endif()
    endif()
endfunction()

# Test executable
add_executable(run_tests
    tests/CondtionalTransitiontest.cpp
    tests/NodeTests.cpp
    tests/BatchNodeTests.cpp
    tests/AsyncNodeTests.cpp
    tests/BaseNodeConcurrencyTests.cpp
    tests/ErrorHandlingTests.cpp
    tests/TypedDataTests.cpp
    tests/BatchDataTests.cpp
    tests/BatchProcessingPerformanceTests.cpp
    tests/AsyncTimeoutTests.cpp
    tests/SecurityValidationTests.cpp
    tests/BuildSystemValidationTests.cpp
    tests/BuildSystemIntegrationTests.cpp
    tests/MoveSemanticsAndConstCorrectnessTests.cpp
    tests/IntegrationWorkflowTestsSimple.cpp
)

target_link_libraries(run_tests PRIVATE
    agentic_framework
    GTest::gtest_main
    benchmark::benchmark
)

configure_executable(run_tests)

# Separate diagnostic system tests executable
add_executable(run_diagnostic_tests
    tests/DiagnosticSystemTests.cpp
)

target_link_libraries(run_diagnostic_tests PRIVATE
    agentic_framework
    GTest::gtest_main
    gmock
)

configure_executable(run_diagnostic_tests)

# Separate move semantics and const-correctness tests executable
add_executable(run_move_semantics_tests
    tests/MoveSemanticsAndConstCorrectnessTests.cpp
)

target_link_libraries(run_move_semantics_tests PRIVATE
    agentic_framework
    GTest::gtest_main
)

configure_executable(run_move_semantics_tests)

# Separate benchmark executable
add_executable(run_benchmarks
    tests/BatchProcessingBenchmarks.cpp
)

target_link_libraries(run_benchmarks PRIVATE
    agentic_framework
    benchmark::benchmark
    benchmark::benchmark_main
)

configure_executable(run_benchmarks)

# Concurrency and performance benchmarks
add_executable(run_concurrency_benchmarks
    tests/RunConcurrencyBenchmarks.cpp
)

target_link_libraries(run_concurrency_benchmarks PRIVATE
    agentic_framework
    GTest::gtest_main
    benchmark::benchmark
)

configure_executable(run_concurrency_benchmarks)

# OptimizedBatchProcessor example executable
add_executable(optimized_batch_example
    examples/OptimizedBatchProcessorExample.cpp
)

target_link_libraries(optimized_batch_example PRIVATE
    agentic_framework
)

configure_executable(optimized_batch_example)

# AsyncTimeout example executable
add_executable(async_timeout_example
    examples/AsyncTimeoutExample.cpp
)

target_link_libraries(async_timeout_example PRIVATE
    agentic_framework
)

configure_executable(async_timeout_example)

# DiagnosticSystem example executable
add_executable(diagnostic_system_example
    examples/DiagnosticSystemExample.cpp
)

target_link_libraries(diagnostic_system_example PRIVATE
    agentic_framework
)

configure_executable(diagnostic_system_example)

# Test discovery
include(GoogleTest)
gtest_discover_tests(run_tests)

# Custom targets for development workflow
if(ENABLE_STATIC_ANALYSIS)
    # Custom target to run static analysis on all sources
    add_custom_target(static_analysis
        COMMENT "Running static analysis tools..."
    )
    
    if(CLANG_TIDY_EXE)
        add_custom_target(clang_tidy
            COMMAND ${CLANG_TIDY_EXE}
                -checks=-*,bugprone-*,cert-*,clang-analyzer-*,cppcoreguidelines-*,hicpp-*,misc-*,modernize-*,performance-*,portability-*,readability-*
                -header-filter=.*
                -p ${CMAKE_BINARY_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/**/*.cpp
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running clang-tidy analysis..."
        )
        add_dependencies(static_analysis clang_tidy)
    endif()
    
    if(CPPCHECK_EXE)
        add_custom_target(cppcheck
            COMMAND ${CPPCHECK_EXE}
                --enable=all
                --std=c++20
                --verbose
                --error-exitcode=1
                --inline-suppr
                --suppress=missingIncludeSystem
                --suppress=unmatchedSuppression
                --suppress=unusedFunction
                --project=${CMAKE_BINARY_DIR}/compile_commands.json
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running cppcheck analysis..."
        )
        add_dependencies(static_analysis cppcheck)
    endif()
endif()

# Generate compile_commands.json for static analysis tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Custom target for running all tests with different sanitizer configurations
add_custom_target(test_all
    COMMENT "Running comprehensive test suite..."
)

add_custom_target(test_with_sanitizers
    COMMAND ${CMAKE_COMMAND} -DENABLE_SANITIZERS=ON -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR}/sanitizer_build
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/sanitizer_build
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR}/sanitizer_build
    COMMENT "Running tests with AddressSanitizer and UBSanitizer..."
)

add_custom_target(test_with_tsan
    COMMAND ${CMAKE_COMMAND} -DENABLE_THREAD_SANITIZER=ON -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR}/tsan_build
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/tsan_build
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR}/tsan_build
    COMMENT "Running tests with ThreadSanitizer..."
)

add_dependencies(test_all test_with_sanitizers test_with_tsan)

# Print configuration summary
message(STATUS "=== Agentic Framework Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Shared libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "Sanitizers: ${ENABLE_SANITIZERS}")
message(STATUS "ThreadSanitizer: ${ENABLE_THREAD_SANITIZER}")
message(STATUS "Static analysis: ${ENABLE_STATIC_ANALYSIS}")
message(STATUS "Warnings as errors: ${TREAT_WARNINGS_AS_ERRORS}")
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
endif()
if(CPPCHECK_EXE)
    message(STATUS "cppcheck: ${CPPCHECK_EXE}")
endif()
message(STATUS "==============================================")
