#!/bin/bash

# Script to run integration tests for the C++ Agentic Framework
# This demonstrates the end-to-end workflow testing capabilities

echo "=========================================="
echo "C++ Agentic Framework Integration Tests"
echo "=========================================="
echo ""

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Build directory not found. Creating and configuring..."
    mkdir build
    cd build
    cmake ..
    cd ..
fi

# Build the project
echo "Building the project..."
cd build
make run_tests -j$(nproc) 2>/dev/null

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    echo ""
    
    # Run the integration tests
    echo "Running Integration Tests..."
    echo "----------------------------"
    
    # Run only the integration workflow tests
    ./run_tests --gtest_filter="SimpleIntegrationWorkflowTest.*" --gtest_color=yes
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "✅ All integration tests passed!"
        echo ""
        echo "Integration Test Summary:"
        echo "========================"
        echo "✓ Complete data processing pipelines with multiple nodes"
        echo "✓ Async workflow execution with proper error handling"
        echo "✓ Batch processing workflows with large datasets"
        echo "✓ Performance benchmarks for realistic usage scenarios"
        echo ""
        echo "The integration tests demonstrate:"
        echo "- End-to-end workflow execution"
        echo "- Error handling and propagation"
        echo "- Batch processing capabilities"
        echo "- Performance measurement and benchmarking"
        echo "- Resource management and cleanup"
    else
        echo "❌ Some integration tests failed. Check the output above for details."
        exit 1
    fi
else
    echo "❌ Build failed. Please check the compilation errors above."
    echo ""
    echo "Note: The framework may have some existing compilation issues"
    echo "that need to be resolved before running integration tests."
    exit 1
fi

cd ..
echo ""
echo "Integration test execution completed."