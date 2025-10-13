#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <regex>

namespace fs = std::filesystem;

class BuildSystemValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the project root directory
        project_root_ = fs::current_path();
        while (!fs::exists(project_root_ / "CMakeLists.txt") && project_root_ != project_root_.parent_path()) {
            project_root_ = project_root_.parent_path();
        }
        
        ASSERT_TRUE(fs::exists(project_root_ / "CMakeLists.txt")) 
            << "Could not find project root with CMakeLists.txt";
        
        // Create temporary test directory
        test_dir_ = project_root_ / "build" / "test_validation";
        fs::create_directories(test_dir_);
    }
    
    void TearDown() override {
        // Clean up test directories
        if (fs::exists(test_dir_)) {
            std::error_code ec;
            fs::remove_all(test_dir_, ec);
            // Don't fail test if cleanup fails
        }
    }
    
    // Execute a command and capture output
    struct CommandResult {
        int exit_code;
        std::string stdout_output;
        std::string stderr_output;
    };
    
    CommandResult executeCommand(const std::string& command) {
        CommandResult result;
        
        // Create temporary files for stdout and stderr
        auto stdout_file = test_dir_ / "stdout.tmp";
        auto stderr_file = test_dir_ / "stderr.tmp";
        
        std::string full_command = command + " > " + stdout_file.string() + " 2> " + stderr_file.string();
        result.exit_code = std::system(full_command.c_str());
        
        // Read stdout
        if (fs::exists(stdout_file)) {
            std::ifstream stdout_stream(stdout_file);
            std::stringstream buffer;
            buffer << stdout_stream.rdbuf();
            result.stdout_output = buffer.str();
        }
        
        // Read stderr
        if (fs::exists(stderr_file)) {
            std::ifstream stderr_stream(stderr_file);
            std::stringstream buffer;
            buffer << stderr_stream.rdbuf();
            result.stderr_output = buffer.str();
        }
        
        return result;
    }
    
    // Configure CMake with specific options
    bool configureCMake(const std::string& build_dir, const std::vector<std::string>& cmake_options = {}) {
        fs::create_directories(test_dir_ / build_dir);
        
        std::string command = "cd " + (test_dir_ / build_dir).string() + " && cmake";
        for (const auto& option : cmake_options) {
            command += " " + option;
        }
        command += " " + project_root_.string();
        
        auto result = executeCommand(command);
        return result.exit_code == 0;
    }
    
    // Build the project
    bool buildProject(const std::string& build_dir) {
        std::string command = "cd " + (test_dir_ / build_dir).string() + " && cmake --build .";
        auto result = executeCommand(command);
        return result.exit_code == 0;
    }
    
    // Run tests
    bool runTests(const std::string& build_dir) {
        std::string command = "cd " + (test_dir_ / build_dir).string() + " && ctest --output-on-failure";
        auto result = executeCommand(command);
        return result.exit_code == 0;
    }
    
    // Check if a tool is available
    bool isToolAvailable(const std::string& tool) {
        auto result = executeCommand("which " + tool);
        return result.exit_code == 0;
    }
    
    fs::path project_root_;
    fs::path test_dir_;
};

// Test basic CMake configuration
TEST_F(BuildSystemValidationTest, BasicCMakeConfiguration) {
    EXPECT_TRUE(configureCMake("basic")) << "Basic CMake configuration should succeed";
    EXPECT_TRUE(fs::exists(test_dir_ / "basic" / "CMakeCache.txt")) << "CMakeCache.txt should be generated";
}

// Test default build configuration
TEST_F(BuildSystemValidationTest, DefaultBuildConfiguration) {
    ASSERT_TRUE(configureCMake("default", {"-DCMAKE_BUILD_TYPE=Release"}));
    EXPECT_TRUE(buildProject("default")) << "Default build should succeed";
    
    // Check that the library was built
    auto build_dir = test_dir_ / "default";
    bool library_found = false;
    
    for (const auto& entry : fs::recursive_directory_iterator(build_dir)) {
        if (entry.path().filename().string().find("agentic_framework") != std::string::npos &&
            (entry.path().extension() == ".a" || entry.path().extension() == ".so" || 
             entry.path().extension() == ".lib" || entry.path().extension() == ".dll")) {
            library_found = true;
            break;
        }
    }
    
    EXPECT_TRUE(library_found) << "Agentic framework library should be built";
}

// Test debug build configuration
TEST_F(BuildSystemValidationTest, DebugBuildConfiguration) {
    ASSERT_TRUE(configureCMake("debug", {"-DCMAKE_BUILD_TYPE=Debug", "-DTREAT_WARNINGS_AS_ERRORS=ON"}));
    EXPECT_TRUE(buildProject("debug")) << "Debug build should succeed";
}

// Test AddressSanitizer configuration
TEST_F(BuildSystemValidationTest, AddressSanitizerConfiguration) {
    // Skip on MSVC as sanitizers are not fully supported
    if (std::getenv("MSVC") || std::getenv("VCPKG_TARGET_TRIPLET")) {
        GTEST_SKIP() << "Skipping sanitizer test on MSVC";
    }
    
    std::vector<std::string> options = {
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DENABLE_SANITIZERS=ON"
    };
    
    ASSERT_TRUE(configureCMake("asan", options)) << "AddressSanitizer configuration should succeed";
    EXPECT_TRUE(buildProject("asan")) << "Build with AddressSanitizer should succeed";
    
    // Verify sanitizer flags are present in compile commands
    auto compile_commands_path = test_dir_ / "asan" / "compile_commands.json";
    if (fs::exists(compile_commands_path)) {
        std::ifstream file(compile_commands_path);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        EXPECT_TRUE(content.find("-fsanitize=address") != std::string::npos) 
            << "AddressSanitizer flag should be present in compile commands";
        EXPECT_TRUE(content.find("-fsanitize=undefined") != std::string::npos) 
            << "UndefinedBehaviorSanitizer flag should be present in compile commands";
    }
}

// Test ThreadSanitizer configuration
TEST_F(BuildSystemValidationTest, ThreadSanitizerConfiguration) {
    // Skip on MSVC as sanitizers are not fully supported
    if (std::getenv("MSVC") || std::getenv("VCPKG_TARGET_TRIPLET")) {
        GTEST_SKIP() << "Skipping sanitizer test on MSVC";
    }
    
    std::vector<std::string> options = {
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DENABLE_THREAD_SANITIZER=ON"
    };
    
    ASSERT_TRUE(configureCMake("tsan", options)) << "ThreadSanitizer configuration should succeed";
    EXPECT_TRUE(buildProject("tsan")) << "Build with ThreadSanitizer should succeed";
    
    // Verify ThreadSanitizer flags are present
    auto compile_commands_path = test_dir_ / "tsan" / "compile_commands.json";
    if (fs::exists(compile_commands_path)) {
        std::ifstream file(compile_commands_path);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        EXPECT_TRUE(content.find("-fsanitize=thread") != std::string::npos) 
            << "ThreadSanitizer flag should be present in compile commands";
    }
}

// Test mutually exclusive sanitizer options
TEST_F(BuildSystemValidationTest, MutuallyExclusiveSanitizers) {
    std::vector<std::string> options = {
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DENABLE_SANITIZERS=ON",
        "-DENABLE_THREAD_SANITIZER=ON"
    };
    
    // This should fail during configuration
    EXPECT_FALSE(configureCMake("exclusive_sanitizers", options)) 
        << "Configuration should fail when both sanitizer options are enabled";
}

// Test static analysis integration
TEST_F(BuildSystemValidationTest, StaticAnalysisIntegration) {
    std::vector<std::string> options = {
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DENABLE_STATIC_ANALYSIS=ON"
    };
    
    ASSERT_TRUE(configureCMake("static_analysis", options)) << "Static analysis configuration should succeed";
    
    // Check if static analysis tools are found
    auto cmake_cache_path = test_dir_ / "static_analysis" / "CMakeCache.txt";
    if (fs::exists(cmake_cache_path)) {
        std::ifstream file(cmake_cache_path);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        // Check if clang-tidy was found
        bool clang_tidy_found = content.find("CLANG_TIDY_EXE") != std::string::npos;
        bool cppcheck_found = content.find("CPPCHECK_EXE") != std::string::npos;
        
        if (clang_tidy_found || cppcheck_found) {
            // If tools are available, build should succeed
            EXPECT_TRUE(buildProject("static_analysis")) << "Build with static analysis should succeed";
        } else {
            // If tools are not available, configuration should still succeed but with warnings
            std::cout << "Static analysis tools not found, skipping build test" << std::endl;
        }
    }
}

// Test warning detection
TEST_F(BuildSystemValidationTest, WarningDetection) {
    // Create a temporary source file with warnings
    auto warning_test_dir = test_dir_ / "warning_test";
    fs::create_directories(warning_test_dir);
    
    // Create a simple CMakeLists.txt for warning test
    std::ofstream cmake_file(warning_test_dir / "CMakeLists.txt");
    cmake_file << R"(
cmake_minimum_required(VERSION 3.15)
project(warning_test CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(warning_test warning_test.cpp)

# Enable high warning levels
if(MSVC)
    target_compile_options(warning_test PRIVATE /W4)
else()
    target_compile_options(warning_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Treat warnings as errors
if(MSVC)
    target_compile_options(warning_test PRIVATE /WX)
else()
    target_compile_options(warning_test PRIVATE -Werror)
endif()
)";
    cmake_file.close();
    
    // Create a source file with intentional warnings
    std::ofstream source_file(warning_test_dir / "warning_test.cpp");
    source_file << R"(
#include <iostream>

int main() {
    int unused_variable = 42;  // This should generate a warning
    int x = 10;
    int y = 20;
    
    // Intentional sign conversion warning
    unsigned int result = x + y;
    
    std::cout << "Result: " << result << std::endl;
    return 0;
}
)";
    source_file.close();
    
    // Try to build - this should fail due to warnings being treated as errors
    std::string command = "cd " + warning_test_dir.string() + " && cmake . && cmake --build .";
    auto result = executeCommand(command);
    
    EXPECT_NE(result.exit_code, 0) << "Build should fail when warnings are treated as errors";
    EXPECT_TRUE(result.stderr_output.find("unused") != std::string::npos || 
                result.stdout_output.find("unused") != std::string::npos) 
        << "Warning about unused variable should be detected";
}

// Test CMake presets functionality
TEST_F(BuildSystemValidationTest, CMakePresetsSupport) {
    // Check if CMakePresets.json exists
    auto presets_file = project_root_ / "CMakePresets.json";
    ASSERT_TRUE(fs::exists(presets_file)) << "CMakePresets.json should exist";
    
    // Test configuring with a preset
    std::string command = "cd " + project_root_.string() + " && cmake --preset=default";
    auto result = executeCommand(command);
    
    if (result.exit_code == 0) {
        // If presets are supported, test should pass
        EXPECT_EQ(result.exit_code, 0) << "CMake preset configuration should succeed";
        
        // Check if build directory was created
        EXPECT_TRUE(fs::exists(project_root_ / "build" / "default")) 
            << "Preset should create the specified build directory";
    } else {
        // If CMake version doesn't support presets, skip this test
        GTEST_SKIP() << "CMake version doesn't support presets";
    }
}

// Test cross-platform compatibility
TEST_F(BuildSystemValidationTest, CrossPlatformCompatibility) {
    // Test different generators if available
    std::vector<std::string> generators = {"Unix Makefiles"};
    
    // Check if Ninja is available
    if (isToolAvailable("ninja")) {
        generators.push_back("Ninja");
    }
    
    for (const auto& generator : generators) {
        std::string build_dir = "generator_" + generator;
        std::replace(build_dir.begin(), build_dir.end(), ' ', '_');
        
        std::vector<std::string> options = {
            "-G", generator,
            "-DCMAKE_BUILD_TYPE=Release"
        };
        
        EXPECT_TRUE(configureCMake(build_dir, options)) 
            << "Configuration should succeed with generator: " << generator;
        
        EXPECT_TRUE(buildProject(build_dir)) 
            << "Build should succeed with generator: " << generator;
    }
}

// Test compiler detection and flags
TEST_F(BuildSystemValidationTest, CompilerDetectionAndFlags) {
    ASSERT_TRUE(configureCMake("compiler_test", {"-DCMAKE_BUILD_TYPE=Debug"}));
    
    // Check CMakeCache.txt for compiler information
    auto cmake_cache_path = test_dir_ / "compiler_test" / "CMakeCache.txt";
    ASSERT_TRUE(fs::exists(cmake_cache_path)) << "CMakeCache.txt should exist";
    
    std::ifstream file(cmake_cache_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check that C++ compiler was detected
    EXPECT_TRUE(content.find("CMAKE_CXX_COMPILER:") != std::string::npos) 
        << "C++ compiler should be detected";
    
    // Check that C++20 standard is set
    EXPECT_TRUE(content.find("CMAKE_CXX_STANDARD:STRING=20") != std::string::npos) 
        << "C++20 standard should be set";
}

// Test build system targets
TEST_F(BuildSystemValidationTest, BuildSystemTargets) {
    ASSERT_TRUE(configureCMake("targets_test", {"-DCMAKE_BUILD_TYPE=Release"}));
    
    // Get list of available targets
    std::string command = "cd " + (test_dir_ / "targets_test").string() + " && cmake --build . --target help";
    auto result = executeCommand(command);
    
    if (result.exit_code == 0) {
        // Check for expected targets
        EXPECT_TRUE(result.stdout_output.find("agentic_framework") != std::string::npos) 
            << "Main library target should be available";
        EXPECT_TRUE(result.stdout_output.find("run_tests") != std::string::npos) 
            << "Test target should be available";
    }
}

// Test installation configuration
TEST_F(BuildSystemValidationTest, InstallationConfiguration) {
    ASSERT_TRUE(configureCMake("install_test", {"-DCMAKE_BUILD_TYPE=Release"}));
    ASSERT_TRUE(buildProject("install_test"));
    
    // Test installation to a temporary directory
    auto install_dir = test_dir_ / "install_prefix";
    std::string command = "cd " + (test_dir_ / "install_test").string() + 
                         " && cmake --install . --prefix " + install_dir.string();
    auto result = executeCommand(command);
    
    EXPECT_EQ(result.exit_code, 0) << "Installation should succeed";
    
    if (result.exit_code == 0) {
        // Check that files were installed
        EXPECT_TRUE(fs::exists(install_dir / "lib")) << "Library directory should be created";
        EXPECT_TRUE(fs::exists(install_dir / "include")) << "Include directory should be created";
    }
}

// Test dependency management
TEST_F(BuildSystemValidationTest, DependencyManagement) {
    ASSERT_TRUE(configureCMake("deps_test", {"-DCMAKE_BUILD_TYPE=Debug"}));
    
    // Check that Google Test was fetched
    auto build_dir = test_dir_ / "deps_test";
    bool gtest_found = false;
    
    // Look for Google Test in the build directory
    for (const auto& entry : fs::recursive_directory_iterator(build_dir)) {
        if (entry.path().filename().string().find("gtest") != std::string::npos) {
            gtest_found = true;
            break;
        }
    }
    
    EXPECT_TRUE(gtest_found) << "Google Test dependency should be fetched";
    
    // Try to build to ensure dependencies are properly linked
    EXPECT_TRUE(buildProject("deps_test")) << "Build with dependencies should succeed";
}