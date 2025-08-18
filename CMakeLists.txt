cmake_minimum_required(VERSION 3.23)
project(agentic_framework VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
add_library(agentic_framework STATIC)


# Add after target definition
target_compile_features(agentic_framework PUBLIC cxx_std_17)

# Optional: Add compiler-specific flags for better optimization/warnings
if(MSVC)
    target_compile_options(agentic_framework PRIVATE /W4)
else()
    target_compile_options(agentic_framework PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add sources using target_sources for better organization
target_sources(agentic_framework PRIVATE
    BaseNode/BaseNode.cpp
    BaseNode/ConditionalTransition.cpp
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
