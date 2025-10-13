# C++ Agentic Framework

A modern, production-ready C++20 framework for building high-performance, thread-safe data processing pipelines. This framework provides a comprehensive architecture for creating complex workflows with support for synchronous and asynchronous execution, batch processing, type safety, and robust error handling.

## Features

- **🏗️ Node-based Architecture**: Build complex workflows using interconnected processing nodes
- **⚡ Asynchronous Processing**: Full support for async operations with futures, timeouts, and cancellation
- **📦 Batch Processing**: Efficient handling of large datasets with memory optimization
- **🔄 Parallel Execution**: Multi-threaded batch processing with work stealing and load balancing
- **🌊 Flow Management**: Sophisticated workflow orchestration with conditional routing
- **🛡️ Type Safety**: Compile-time and runtime type checking with validation constraints
- **🔒 Thread Safety**: Built-in synchronization primitives and lock-free data structures
- **🚨 Robust Error Handling**: Comprehensive error propagation with structured error information
- **📊 Performance Monitoring**: Built-in profiling, metrics collection, and memory tracking
- **🔧 Modern C++**: Leverages C++20 features including concepts, coroutines, and RAII patterns

## Documentation

- **[📖 API Reference](docs/API_REFERENCE.md)** - Complete API documentation with examples
- **[🚀 Getting Started Guide](docs/GETTING_STARTED.md)** - Step-by-step tutorial and common patterns
- **[⚡ Performance Tuning](docs/PERFORMANCE_TUNING.md)** - Optimization strategies for high-throughput scenarios
- **[🔄 Concurrency Guide](docs/CONCURRENCY_GUIDE.md)** - Thread safety guarantees and async programming patterns
- **[💡 Usage Examples](examples/comprehensive_usage_examples.cpp)** - Real-world usage scenarios and best practices

## Core Components

### Base Classes

- **BaseNode**: Abstract base class for all nodes in the framework
- **Node**: Standard synchronous processing node
- **Flow**: Workflow management with node orchestration and conditional transitions

### Asynchronous Components

- **AsyncNode**: Base class for asynchronous node operations
- **AsyncFlow**: Asynchronous workflow management with parallel node execution

### Batch Processing

- **BatchNode**: Handles batch data processing operations
- **BatchFlow**: Workflow management for batch operations
- **AsyncBatchFlow**: Asynchronous batch processing workflows
- **AsyncParallelBatchFlow**: Parallel batch processing with async execution

### Specialized Nodes

- **AsyncBatchNode**: Single node for asynchronous batch operations
- **AsyncParallelBatchNode**: Single node for parallel asynchronous batch operations

## Architecture

The framework follows a hierarchical inheritance model:
```
BaseNode (Abstract Base) ├── Node (Basic Processing Unit) │ ├── AsyncNode (Asynchronous Processing) │ │ ├── AsyncBatchNode (AsyncNode + BatchNode) │ │ └── AsyncParallelBatchNode (AsyncNode + BatchNode) │ └── BatchNode (Batch Processing) │ ├── AsyncBatchNode (AsyncNode + BatchNode) │ └── AsyncParallelBatchNode (AsyncNode + BatchNode) │ └── Flow (Workflow Orchestration) ├── AsyncFlow (Flow + AsyncNode) │ ├── AsyncBatchFlow (AsyncFlow + BatchFlow) │ └── AsyncParallelBatchFlow (AsyncFlow + BatchFlow) └── BatchFlow (Flow + Batch Processing) ├── AsyncBatchFlow (AsyncFlow + BatchFlow) └── AsyncParallelBatchFlow (AsyncFlow + BatchFlow)
```

## Execution Models

### Synchronous Models
```
BaseNode → Node → BatchNode BaseNode → Flow → BatchFlow
```
### Asynchronous Models
```
BaseNode → Node → AsyncNode BaseNode → Flow → AsyncFlow

```

### Hybrid Models (Multiple Inheritance)
```
AsyncBatchNode: AsyncNode + BatchNode 
AsyncParallelBatchNode: AsyncNode + BatchNode (with parallel execution) 
AsyncBatchFlow: AsyncFlow + BatchFlow AsyncParallelBatchFlow: AsyncFlow + BatchFlow (with parallel execution)
```


## Key Design Patterns

### 1. **Virtual Inheritance**
Used to prevent diamond inheritance problems in complex hierarchies:
- `Flow` uses virtual inheritance from `BaseNode`
- `AsyncFlow` uses virtual inheritance from both `Flow` and `AsyncNode`

### 2. **Multiple Inheritance**
Combines orthogonal capabilities:
- Async + Batch functionality
- Flow + Node capabilities
- Parallel + Batch processing

### 3. **Template-like Flexibility**
Different combinations provide specialized behavior:
- **Sequential**: Standard processing
- **Async**: Non-blocking operations
- **Batch**: Collection processing
- **Parallel**: Concurrent execution

## Building

This project uses CMake for building:

```bash
mkdir build
cd build
cmake ..
make
```

### Requirements

- **C++20 compatible compiler** (GCC 10+, Clang 12+, MSVC 2019+)
- **CMake 3.15 or later**
- **Optional**: Sanitizers for development (AddressSanitizer, ThreadSanitizer, UBSanitizer)

## Quick Start

### Basic Node Processing
```cpp
#include "Node/Node.h"
#include "Flow/StringData.h"

// Create a custom processing node
class MyProcessorNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* input = prep_res->as<StringData>()) {
            return std::make_shared<StringData>("Processed: " + input->value);
        }
        throw std::invalid_argument("Expected StringData");
    }
};

// Use the node
auto node = std::make_shared<MyProcessorNode>();
auto input = std::make_shared<StringData>("Hello World");
auto result = node->Execute(input);
```

### Asynchronous Processing with Timeout
```cpp
#include "AsyncNode/AsyncNode.h"

class AsyncProcessorNode : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [prep_res]() {
            // Simulate async work
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            if (auto* input = prep_res->as<StringData>()) {
                return std::static_pointer_cast<FrameworkData>(
                    std::make_shared<StringData>("Async processed: " + input->value)
                );
            }
            throw std::invalid_argument("Expected StringData");
        });
    }
};

// Use with timeout
auto async_node = std::make_shared<AsyncProcessorNode>();
auto input = std::make_shared<StringData>("async data");

try {
    auto future = async_node->RunAsyncWithTimeout(input, std::chrono::seconds(5));
    auto result = future.get();
} catch (const std::runtime_error& e) {
    std::cout << "Operation timed out" << std::endl;
}
```

### Error Handling with Result Types
```cpp
#include "BaseNode/Result.h"

using namespace framework;

Result<std::shared_ptr<FrameworkData>> safe_process(std::shared_ptr<FrameworkData> input) {
    if (!input) {
        return FrameworkError::invalid_argument("Input cannot be null");
    }
    
    // Process and return result or error
    return std::make_shared<StringData>("Safely processed");
}

// Chain operations with automatic error propagation
auto result = safe_process(input)
    .map([](auto data) { return transform_data(data); })
    .flat_map([](auto data) { return validate_data(data); })
    .value_or(std::make_shared<StringData>("Default value"));
```

## Component Capabilities Matrix

| Component | Sync | Async | Batch | Parallel | Workflow |
|-----------|------|-------|-------|----------|----------|
| Node | ✓ | ✗ | ✗ | ✗ | ✗ |
| AsyncNode | ✓ | ✓ | ✗ | ✗ | ✗ |
| BatchNode | ✓ | ✗ | ✓ | ✗ | ✗ |
| AsyncBatchNode | ✓ | ✓ | ✓ | ✗ | ✗ |
| AsyncParallelBatchNode | ✓ | ✓ | ✓ | ✓ | ✗ |
| Flow | ✓ | ✗ | ✗ | ✗ | ✓ |
| AsyncFlow | ✓ | ✓ | ✗ | ✗ | ✓ |
| BatchFlow | ✓ | ✗ | ✓ | ✗ | ✓ |
| AsyncBatchFlow | ✓ | ✓ | ✓ | ✗ | ✓ |
| AsyncParallelBatchFlow | ✓ | ✓ | ✓ | ✓ | ✓ |

## Thread Safety Guarantees

- **BaseNode**: All parameter and successor operations are thread-safe using `std::shared_mutex`
- **AsyncNode**: Prevents concurrent execution of the same node instance with atomic guards
- **FrameworkData**: Immutable after construction, safe for concurrent read access
- **TypedData**: Thread-safe for read operations, requires external synchronization for writes
- **Result**: All operations are thread-safe and can be used across threads
- **Batch Processing**: Designed for safe concurrent processing with proper synchronization

See the [Concurrency Guide](docs/CONCURRENCY_GUIDE.md) for detailed thread safety information.

## Key Concepts

### Data Flow Architecture
The framework uses a strongly-typed `FrameworkData` hierarchy for safe data passing between nodes:
- **Type Safety**: Compile-time and runtime type checking with safe casting helpers
- **Polymorphism**: Virtual interface for extensible data types
- **Validation**: Built-in constraint checking and validation mechanisms
- **Serialization**: Optional serialization support for data persistence

### Execution Models
1. **Synchronous**: Traditional blocking execution with retry mechanisms
2. **Asynchronous**: Non-blocking execution with futures, timeouts, and cancellation
3. **Batch**: Efficient processing of data collections with memory optimization
4. **Parallel Batch**: Concurrent processing with work stealing and load balancing

### Node Orchestration
Flows provide sophisticated workflow management:
- **Conditional Routing**: Dynamic path selection based on processing results
- **Parameter Management**: Thread-safe configuration and state management
- **Error Recovery**: Comprehensive error handling with fallback mechanisms
- **Resource Management**: RAII-based cleanup and resource tracking

### Error Handling Strategy
The framework provides multiple layers of error handling:
- **FrameworkError**: Structured error information with context and error codes
- **Result<T>**: Monadic error handling for operations that can fail
- **Exception Safety**: Strong exception safety guarantees throughout
- **Async Error Propagation**: Proper error handling in asynchronous contexts

## Performance Characteristics

The framework is optimized for high-throughput scenarios:
- **Memory Efficiency**: Object pooling, memory mapping, and optimized allocations
- **Concurrency**: Lock-free data structures and work-stealing thread pools
- **SIMD Support**: Vectorized operations for batch processing where applicable
- **Cache Optimization**: Data locality improvements and prefetching strategies

See the [Performance Tuning Guide](docs/PERFORMANCE_TUNING.md) for optimization strategies.

## Testing and Quality Assurance

- **Comprehensive Test Suite**: Unit tests, integration tests, and performance benchmarks
- **Sanitizer Integration**: AddressSanitizer, ThreadSanitizer, and UBSanitizer support
- **Static Analysis**: Integration with clang-tidy and cppcheck
- **Memory Leak Detection**: Built-in memory tracking and leak detection
- **Concurrency Testing**: Stress tests for thread safety and race condition detection

## Contributing

This framework is designed to be extensible and welcomes contributions:

1. **Custom Node Types**: Inherit from base classes to create specialized processors
2. **Flow Patterns**: Implement custom workflow orchestration logic  
3. **Data Types**: Extend the FrameworkData hierarchy for domain-specific data
4. **Performance Optimizations**: Contribute platform-specific optimizations
5. **Documentation**: Help improve documentation and examples

Please see our contribution guidelines and ensure all tests pass before submitting pull requests.

## License

This project is licensed under the terms specified in the [LICENSE](LICENSE) file.

## Dependencies

- **Standard C++20 Library**: Core language features and standard library
- **Threading Support**: `<future>`, `<async>`, `<mutex>`, `<atomic>`
- **Type Safety**: `<typeinfo>`, `<type_traits>`, `<concepts>`
- **Memory Management**: `<memory>`, `<memory_resource>`
- **Optional Dependencies**:
  - **Sanitizers**: For development and testing
  - **Benchmark Library**: For performance testing
  - **Threading Building Blocks**: For advanced parallel algorithms