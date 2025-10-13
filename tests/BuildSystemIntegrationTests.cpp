#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <regex>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

class BuildSystemIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Find project root
        project_root_ = fs::current_path();
        while (!fs::exists(project_root_ / "CMakeLists.txt") && project_root_ != project_root_.parent_path()) {
            project_root_ = project_root_.parent_path();
        }
        
        ASSERT_TRUE(fs::exists(project_root_ / "CMakeLists.txt")) 
            << "Could not find project root with CMakeLists.txt";
    }
    
    fs::path project_root_;
};

// Test that CMakeLists.txt contains required modern CMake features
TEST_F(BuildSystemIntegrationTest, CMakeListsModernFeatures) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    ASSERT_TRUE(fs::exists(cmake_file));
    
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for modern CMake version requirement
    EXPECT_TRUE(content.find("cmake_minimum_required(VERSION 3.15)") != std::string::npos)
        << "CMakeLists.txt should require CMake 3.15 or later";
    
    // Check for C++20 standard
    EXPECT_TRUE(content.find("CMAKE_CXX_STANDARD 20") != std::string::npos)
        << "CMakeLists.txt should set C++20 standard";
    
    // Check for sanitizer options
    EXPECT_TRUE(content.find("ENABLE_SANITIZERS") != std::string::npos)
        << "CMakeLists.txt should have sanitizer options";
    
    EXPECT_TRUE(content.find("ENABLE_THREAD_SANITIZER") != std::string::npos)
        << "CMakeLists.txt should have ThreadSanitizer option";
    
    // Check for static analysis options
    EXPECT_TRUE(content.find("ENABLE_STATIC_ANALYSIS") != std::string::npos)
        << "CMakeLists.txt should have static analysis options";
    
    // Check for warning configuration
    EXPECT_TRUE(content.find("TREAT_WARNINGS_AS_ERRORS") != std::string::npos)
        << "CMakeLists.txt should have warnings as errors option";
}

// Test that CMakePresets.json is properly configured
TEST_F(BuildSystemIntegrationTest, CMakePresetsConfiguration) {
    auto presets_file = project_root_ / "CMakePresets.json";
    ASSERT_TRUE(fs::exists(presets_file)) << "CMakePresets.json should exist";
    
    std::ifstream file(presets_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for required presets
    EXPECT_TRUE(content.find("\"default\"") != std::string::npos)
        << "Should have default preset";
    
    EXPECT_TRUE(content.find("\"debug\"") != std::string::npos)
        << "Should have debug preset";
    
    EXPECT_TRUE(content.find("\"sanitizers\"") != std::string::npos)
        << "Should have sanitizers preset";
    
    EXPECT_TRUE(content.find("\"tsan\"") != std::string::npos)
        << "Should have ThreadSanitizer preset";
    
    EXPECT_TRUE(content.find("\"static-analysis\"") != std::string::npos)
        << "Should have static analysis preset";
    
    EXPECT_TRUE(content.find("\"ci\"") != std::string::npos)
        << "Should have CI preset";
    
    // Check for test presets
    EXPECT_TRUE(content.find("testPresets") != std::string::npos)
        << "Should have test presets configuration";
}

// Test build script functionality
TEST_F(BuildSystemIntegrationTest, BuildScriptExists) {
    auto build_script = project_root_ / "build.sh";
    ASSERT_TRUE(fs::exists(build_script)) << "build.sh should exist";
    
    // Check if script is executable
    auto perms = fs::status(build_script).permissions();
    EXPECT_TRUE((perms & fs::perms::owner_exec) != fs::perms::none)
        << "build.sh should be executable";
    
    // Read script content and check for required functions
    std::ifstream file(build_script);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find("configure_build") != std::string::npos)
        << "build.sh should have configure_build function";
    
    EXPECT_TRUE(content.find("build_project") != std::string::npos)
        << "build.sh should have build_project function";
    
    EXPECT_TRUE(content.find("run_tests") != std::string::npos)
        << "build.sh should have run_tests function";
    
    EXPECT_TRUE(content.find("run_sanitizers") != std::string::npos)
        << "build.sh should have run_sanitizers function";
    
    EXPECT_TRUE(content.find("run_static_analysis") != std::string::npos)
        << "build.sh should have run_static_analysis function";
}

// Test that validation script exists and is properly configured
TEST_F(BuildSystemIntegrationTest, ValidationScriptExists) {
    auto validation_script = project_root_ / "tests" / "validate_build_system.sh";
    ASSERT_TRUE(fs::exists(validation_script)) << "validate_build_system.sh should exist";
    
    // Check if script is executable
    auto perms = fs::status(validation_script).permissions();
    EXPECT_TRUE((perms & fs::perms::owner_exec) != fs::perms::none)
        << "validate_build_system.sh should be executable";
    
    // Read script content and check for required test functions
    std::ifstream file(validation_script);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find("test_basic_cmake_configuration") != std::string::npos)
        << "Validation script should test basic CMake configuration";
    
    EXPECT_TRUE(content.find("test_sanitizer_configurations") != std::string::npos)
        << "Validation script should test sanitizer configurations";
    
    EXPECT_TRUE(content.find("test_static_analysis") != std::string::npos)
        << "Validation script should test static analysis";
    
    EXPECT_TRUE(content.find("test_cross_platform_compatibility") != std::string::npos)
        << "Validation script should test cross-platform compatibility";
}

// Test compiler warning configuration
TEST_F(BuildSystemIntegrationTest, CompilerWarningConfiguration) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for comprehensive warning flags
    EXPECT_TRUE(content.find("-Wall") != std::string::npos)
        << "Should enable -Wall warnings";
    
    EXPECT_TRUE(content.find("-Wextra") != std::string::npos)
        << "Should enable -Wextra warnings";
    
    EXPECT_TRUE(content.find("-Wpedantic") != std::string::npos)
        << "Should enable -Wpedantic warnings";
    
    EXPECT_TRUE(content.find("-Wconversion") != std::string::npos)
        << "Should enable -Wconversion warnings";
    
    EXPECT_TRUE(content.find("-Wshadow") != std::string::npos)
        << "Should enable -Wshadow warnings";
    
    // Check for MSVC warnings
    EXPECT_TRUE(content.find("/W4") != std::string::npos)
        << "Should enable /W4 warnings for MSVC";
    
    // Check for warnings as errors configuration
    EXPECT_TRUE(content.find("-Werror") != std::string::npos)
        << "Should have -Werror configuration";
    
    EXPECT_TRUE(content.find("/WX") != std::string::npos)
        << "Should have /WX configuration for MSVC";
}

// Test sanitizer configuration
TEST_F(BuildSystemIntegrationTest, SanitizerConfiguration) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for AddressSanitizer configuration
    EXPECT_TRUE(content.find("-fsanitize=address") != std::string::npos)
        << "Should configure AddressSanitizer";
    
    // Check for UndefinedBehaviorSanitizer configuration
    EXPECT_TRUE(content.find("-fsanitize=undefined") != std::string::npos)
        << "Should configure UndefinedBehaviorSanitizer";
    
    // Check for ThreadSanitizer configuration
    EXPECT_TRUE(content.find("-fsanitize=thread") != std::string::npos)
        << "Should configure ThreadSanitizer";
    
    // Check for LeakSanitizer configuration
    EXPECT_TRUE(content.find("-fsanitize=leak") != std::string::npos)
        << "Should configure LeakSanitizer";
    
    // Check for frame pointer preservation
    EXPECT_TRUE(content.find("-fno-omit-frame-pointer") != std::string::npos)
        << "Should preserve frame pointers for better stack traces";
    
    // Check for mutual exclusion logic
    EXPECT_TRUE(content.find("ENABLE_SANITIZERS AND ENABLE_THREAD_SANITIZER") != std::string::npos)
        << "Should have mutual exclusion logic for sanitizers";
}

// Test static analysis tool configuration
TEST_F(BuildSystemIntegrationTest, StaticAnalysisConfiguration) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for clang-tidy configuration
    EXPECT_TRUE(content.find("find_program(CLANG_TIDY_EXE") != std::string::npos)
        << "Should search for clang-tidy";
    
    EXPECT_TRUE(content.find("CXX_CLANG_TIDY") != std::string::npos)
        << "Should configure clang-tidy for targets";
    
    // Check for cppcheck configuration
    EXPECT_TRUE(content.find("find_program(CPPCHECK_EXE") != std::string::npos)
        << "Should search for cppcheck";
    
    EXPECT_TRUE(content.find("CXX_CPPCHECK") != std::string::npos)
        << "Should configure cppcheck for targets";
    
    // Check for comprehensive check categories
    EXPECT_TRUE(content.find("bugprone-") != std::string::npos)
        << "Should enable bugprone checks";
    
    EXPECT_TRUE(content.find("modernize-") != std::string::npos)
        << "Should enable modernize checks";
    
    EXPECT_TRUE(content.find("performance-") != std::string::npos)
        << "Should enable performance checks";
    
    EXPECT_TRUE(content.find("readability-") != std::string::npos)
        << "Should enable readability checks";
}

// Test target configuration
TEST_F(BuildSystemIntegrationTest, TargetConfiguration) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for main library target
    EXPECT_TRUE(content.find("add_library(agentic_framework") != std::string::npos)
        << "Should define main library target";
    
    // Check for test targets
    EXPECT_TRUE(content.find("add_executable(run_tests") != std::string::npos)
        << "Should define test executable";
    
    EXPECT_TRUE(content.find("add_executable(run_benchmarks") != std::string::npos)
        << "Should define benchmark executable";
    
    // Check for example targets
    EXPECT_TRUE(content.find("add_executable(optimized_batch_example") != std::string::npos)
        << "Should define example executables";
    
    // Check for target properties
    EXPECT_TRUE(content.find("target_compile_features") != std::string::npos)
        << "Should set compile features";
    
    EXPECT_TRUE(content.find("target_include_directories") != std::string::npos)
        << "Should set include directories";
    
    EXPECT_TRUE(content.find("target_link_libraries") != std::string::npos)
        << "Should link required libraries";
}

// Test dependency management
TEST_F(BuildSystemIntegrationTest, DependencyManagement) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for FetchContent usage
    EXPECT_TRUE(content.find("FetchContent_Declare") != std::string::npos)
        << "Should use FetchContent for dependencies";
    
    EXPECT_TRUE(content.find("FetchContent_MakeAvailable") != std::string::npos)
        << "Should make dependencies available";
    
    // Check for Google Test
    EXPECT_TRUE(content.find("googletest") != std::string::npos)
        << "Should fetch Google Test";
    
    // Check for Google Benchmark
    EXPECT_TRUE(content.find("googlebenchmark") != std::string::npos)
        << "Should fetch Google Benchmark";
    
    // Check for threading support
    EXPECT_TRUE(content.find("find_package(Threads") != std::string::npos)
        << "Should find threading package";
    
    EXPECT_TRUE(content.find("Threads::Threads") != std::string::npos)
        << "Should link threading library";
}

// Test installation configuration
TEST_F(BuildSystemIntegrationTest, InstallationConfiguration) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for installation commands
    EXPECT_TRUE(content.find("install(TARGETS") != std::string::npos)
        << "Should have target installation";
    
    EXPECT_TRUE(content.find("install(EXPORT") != std::string::npos)
        << "Should install export files";
    
    // Check for package configuration
    EXPECT_TRUE(content.find("configure_package_config_file") != std::string::npos)
        << "Should configure package config file";
    
    EXPECT_TRUE(content.find("write_basic_package_version_file") != std::string::npos)
        << "Should write package version file";
    
    // Check for GNUInstallDirs usage
    EXPECT_TRUE(content.find("GNUInstallDirs") != std::string::npos)
        << "Should use GNUInstallDirs for standard paths";
}

// Test custom targets and commands
TEST_F(BuildSystemIntegrationTest, CustomTargetsAndCommands) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for static analysis targets
    EXPECT_TRUE(content.find("add_custom_target(static_analysis") != std::string::npos)
        << "Should have static analysis custom target";
    
    // Check for test targets
    EXPECT_TRUE(content.find("add_custom_target(test_all") != std::string::npos)
        << "Should have comprehensive test target";
    
    EXPECT_TRUE(content.find("add_custom_target(test_with_sanitizers") != std::string::npos)
        << "Should have sanitizer test target";
    
    EXPECT_TRUE(content.find("add_custom_target(test_with_tsan") != std::string::npos)
        << "Should have ThreadSanitizer test target";
    
    // Check for compile commands export
    EXPECT_TRUE(content.find("CMAKE_EXPORT_COMPILE_COMMANDS") != std::string::npos)
        << "Should export compile commands for tools";
}

// Test configuration summary
TEST_F(BuildSystemIntegrationTest, ConfigurationSummary) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for configuration summary output
    EXPECT_TRUE(content.find("Build Configuration") != std::string::npos)
        << "Should print build configuration summary";
    
    EXPECT_TRUE(content.find("message(STATUS") != std::string::npos)
        << "Should use STATUS messages for configuration info";
}

// Test that all required source files are included
TEST_F(BuildSystemIntegrationTest, SourceFileInclusion) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for target_sources usage
    EXPECT_TRUE(content.find("target_sources(agentic_framework") != std::string::npos)
        << "Should use target_sources for better organization";
    
    // Check for FILE_SET HEADERS
    EXPECT_TRUE(content.find("FILE_SET HEADERS") != std::string::npos)
        << "Should use FILE_SET for header installation";
    
    // Verify key source files are included
    std::vector<std::string> required_sources = {
        "BaseNode/BaseNode.cpp",
        "Node/Node.cpp",
        "AsyncNode/AsyncNode.cpp",
        "BatchNode/BatchNode.cpp",
        "Flow/Flow.cpp"
    };
    
    for (const auto& source : required_sources) {
        EXPECT_TRUE(content.find(source) != std::string::npos)
            << "Should include source file: " << source;
    }
}

// Test helper function configuration
TEST_F(BuildSystemIntegrationTest, HelperFunctionConfiguration) {
    auto cmake_file = project_root_ / "CMakeLists.txt";
    std::ifstream file(cmake_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for configure_executable helper function
    EXPECT_TRUE(content.find("function(configure_executable") != std::string::npos)
        << "Should have configure_executable helper function";
    
    // Check that helper function is used for executables
    EXPECT_TRUE(content.find("configure_executable(run_tests)") != std::string::npos)
        << "Should use helper function for test executable";
    
    EXPECT_TRUE(content.find("configure_executable(run_benchmarks)") != std::string::npos)
        << "Should use helper function for benchmark executable";
}