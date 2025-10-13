# Integration Tests for C++ Agentic Framework

This document describes the comprehensive integration tests implemented for the C++ Agentic Framework as part of task 13: "Create integration tests for end-to-end workflow scenarios".

## Overview

The integration tests validate the framework's ability to handle complete data processing pipelines, async workflow execution with proper error handling, batch processing workflows with large datasets, and provide performance benchmarks for realistic usage scenarios.

## Test Structure

### Test Files

1. **`IntegrationWorkflowTestsSimple.cpp`** - Main integration test suite
2. **`run_integration_tests.sh`** - Test runner script
3. **`IntegrationTestsREADME.md`** - This documentation

### Test Classes

#### SimpleIntegrationWorkflowTest
Main test fixture that provides:
- Test data generation (small and medium datasets)
- Performance tracking and measurement
- Resource cleanup verification
- Execution time measurement utilities

#### SimpleDataProcessingNode
Test implementation of AsyncNode that:
- Simulates realistic processing delays
- Supports configurable failure modes
- Processes different data types (int, double, string)
- Demonstrates proper async execution patterns

#### SimpleAggregationNode
Test implementation for batch aggregation that:
- Performs sum and count operations on batches
- Demonstrates batch data processing
- Shows proper type handling in batch contexts

## Test Scenarios

### 1. Complete Data Processing Pipelines (Requirement 10.2)

**Test**: `CompleteDataProcessingPipeline`

**Purpose**: Validates end-to-end workflow execution with multiple connected nodes.

**What it tests**:
- Multi-stage processing pipeline creation
- Node chaining and data flow
- Async execution coordination
- Result propagation through pipeline stages
- Performance measurement and reporting

**Expected behavior**:
- Pipeline processes data through all stages successfully
- Each node receives output from previous node
- Final result reflects all transformations
- Execution completes within reasonable time bounds

### 2. Async Workflow Error Handling (Requirement 6.1)

**Test**: `AsyncWorkflowWithErrorHandling`

**Purpose**: Verifies proper error handling and propagation in async workflows.

**What it tests**:
- Exception propagation through async chains
- Error handling in multi-node workflows
- Graceful failure modes
- Error message preservation
- Recovery mechanisms

**Expected behavior**:
- Exceptions are properly propagated to caller
- Error messages contain meaningful information
- Failed workflows don't leave resources in inconsistent state
- Reliable nodes maintain high success rates

### 3. Batch Processing Workflows (Requirement 4.5)

**Test**: `BatchProcessingWorkflowsLargeDatasets`

**Purpose**: Validates batch processing capabilities with substantial datasets.

**What it tests**:
- Large dataset handling (1000+ items)
- Batch creation and management
- Memory efficiency with large batches
- Batch aggregation operations
- Throughput measurement

**Expected behavior**:
- Processes large batches without memory issues
- Maintains performance with increasing batch sizes
- Aggregation operations produce correct results
- Memory usage remains bounded

### 4. Performance Benchmarks (Requirement 10.3)

**Test**: `PerformanceBenchmarksRealisticScenarios`

**Purpose**: Provides performance benchmarks for realistic usage patterns.

**What it tests**:
- Real-time processing scenarios with concurrent requests
- Sequential batch processing performance
- Throughput measurement (requests/second, items/second)
- Latency characteristics
- Resource utilization patterns

**Scenarios tested**:
- **Real-time Processing**: Multiple concurrent requests processed simultaneously
- **Sequential Batch Processing**: Multiple batches processed in sequence

**Metrics collected**:
- Requests per second
- Items processed per second
- Average, minimum, and maximum processing times
- Success rates and error rates
- Resource cleanup verification

## Performance Expectations

### Real-time Processing
- **Target**: >80% success rate for concurrent requests
- **Measurement**: Requests/second throughput
- **Validation**: All successful requests complete within timeout

### Batch Processing
- **Target**: Linear scaling with batch size
- **Measurement**: Items/second throughput
- **Validation**: Memory usage remains bounded

### Error Handling
- **Target**: 100% error propagation accuracy
- **Measurement**: Exception message preservation
- **Validation**: No resource leaks on failures

## Running the Tests

### Prerequisites
- CMake 3.15+
- C++20 compatible compiler
- Google Test framework (automatically fetched)
- Google Benchmark framework (automatically fetched)

### Quick Start
```bash
# Run all integration tests
./run_integration_tests.sh

# Run specific test suite
cd build
./run_tests --gtest_filter="SimpleIntegrationWorkflowTest.*"

# Run with verbose output
./run_tests --gtest_filter="SimpleIntegrationWorkflowTest.*" --gtest_color=yes -v
```

### Manual Build and Test
```bash
# Configure and build
mkdir -p build
cd build
cmake ..
make run_tests -j$(nproc)

# Run integration tests
./run_tests --gtest_filter="SimpleIntegrationWorkflowTest.*"
```

## Test Output Interpretation

### Success Indicators
- ✅ All tests pass with `[  PASSED  ]` status
- Performance metrics within expected ranges
- Zero resource leaks reported
- Error rates within acceptable bounds

### Performance Metrics
The tests output detailed performance information:

```
Real-time processing results:
  - Total requests: 10
  - Successful requests: 10
  - Total duration: 45 ms
  - Requests/second: 222.22

Sequential batch processing results:
  - Number of batches: 5
  - Average batch size: 20
  - Average processing time: 8.2 ms
  - Min processing time: 7.1 ms
  - Max processing time: 9.8 ms
  - Throughput: 2439.02 items/second
```

### Error Analysis
When errors occur, the tests provide detailed information:
- Exception messages and types
- Error propagation paths
- Resource cleanup status
- Performance impact of errors

## Integration with CI/CD

The integration tests are designed to be run in continuous integration environments:

### Exit Codes
- `0`: All tests passed
- `1`: Test failures or build errors

### Test Reports
- Google Test XML output supported
- Performance metrics logged to stdout
- Resource usage tracking available

### Environment Variables
```bash
# Control test verbosity
export GTEST_COLOR=yes

# Filter specific tests
export GTEST_FILTER="SimpleIntegrationWorkflowTest.CompleteDataProcessingPipeline"

# Enable performance profiling
export ENABLE_PERFORMANCE_PROFILING=1
```

## Extending the Tests

### Adding New Scenarios
1. Create new test methods in `SimpleIntegrationWorkflowTest`
2. Follow the naming convention: `TestName` describing the scenario
3. Include performance measurement and error handling
4. Update this README with the new test description

### Adding New Node Types
1. Inherit from `AsyncNode` or existing test nodes
2. Implement required virtual methods
3. Add appropriate error handling and resource cleanup
4. Include in test scenarios as needed

### Performance Benchmarking
1. Use the provided `measureExecutionTime` utility
2. Track relevant metrics (throughput, latency, resource usage)
3. Set appropriate performance expectations
4. Include in the performance summary output

## Troubleshooting

### Common Issues

**Build Failures**
- Ensure C++20 compiler support
- Check CMake version (3.15+ required)
- Verify all dependencies are available

**Test Failures**
- Check system resources (memory, CPU)
- Verify no other processes interfering
- Review error messages for specific failure causes

**Performance Issues**
- Monitor system load during tests
- Check for memory leaks or resource contention
- Adjust test parameters for system capabilities

### Debug Mode
```bash
# Build with debug information
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make run_tests

# Run with debugger
gdb ./run_tests
```

## Requirements Traceability

This integration test suite addresses the following requirements from the specification:

- **Requirement 10.2**: Integration tests verify end-to-end functionality
- **Requirement 10.3**: Performance benchmarks for realistic scenarios  
- **Requirement 4.5**: Batch processing workflows with large datasets
- **Requirement 6.1**: Async workflow execution with proper error handling

Each test method includes comments referencing the specific requirements it validates, ensuring complete traceability from requirements to implementation to testing.