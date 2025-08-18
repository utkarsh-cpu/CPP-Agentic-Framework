# CPP-Agentic-Framework

A modern C++17 framework for building asynchronous, batch-processing, and workflow-based applications. This framework provides a flexible architecture for creating complex processing pipelines with support for synchronous and asynchronous execution, batch operations, and parallel processing.

## Features

- **Node-based Architecture**: Build complex workflows using interconnected nodes
- **Asynchronous Processing**: Full support for async/await patterns using C++ futures
- **Batch Processing**: Handle collections of data efficiently
- **Parallel Execution**: Run batch operations in parallel for improved performance
- **Flow Management**: Create sophisticated workflows with conditional transitions
- **Type-safe Operations**: Leverages modern C++20 features including `std::any` for flexible data handling

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

- C++20 compatible compiler
- CMake 3.30 or later

## Usage Patterns

### Basic Node Processing
```cpp
// Single item processing
auto node = std::make_shared<Node>();
std::any result = node->Run(data);

// Batch processing
auto batchNode = std::make_shared<BatchNode>();
std::any batchResult = batchNode->Run(batchData);
```

### Asynchronous Processing
```cpp
// Async single processing
auto asyncNode = std::make_shared<AsyncNode>();
std::future<std::any> result = asyncNode->RunAsync(data);

// Async batch processing
auto asyncBatchNode = std::make_shared<AsyncBatchNode>();
std::future<std::any> batchResult = asyncBatchNode->RunAsync(batchData);
```

### Workflow Orchestration
```cpp
// Synchronous workflow
auto flow = std::make_shared<Flow>();
std::any result = flow->run(data);

// Asynchronous workflow
auto asyncFlow = std::make_shared<AsyncFlow>();
std::future<std::any> result = asyncFlow->runAsync(data);

// Parallel batch workflow
auto parallelFlow = std::make_shared<AsyncParallelBatchFlow>();
std::future<std::any> result = parallelFlow->runAsync(batchData);
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

## Thread Safety

- Async operations use `std::async` with `std::launch::async` policy
- Parallel batch processing safely handles concurrent execution
- Share

## Key Concepts
### Data Flow
The framework uses for flexible data passing between nodes, allowing for type-safe operations while maintaining flexibility. `std::any`
### Execution Models
1. **Synchronous**: Traditional blocking execution
2. **Asynchronous**: Non-blocking execution using `std::future`
3. **Batch**: Processing collections of data
4. **Parallel Batch**: Concurrent processing of batch items

### Node Orchestration
Flows manage the execution order of nodes through:
- Conditional transitions between nodes
- Parameter passing and configuration
- Result aggregation and post-processing

## Error Handling
The framework provides clear error handling:
- Async classes throw runtime errors when synchronous methods are called
- Type checking for batch data validation
- Future-based error propagation for async operations

## Thread Safety
- Async operations use with policy `std::async``std::launch::async`
- Parallel batch processing safely handles concurrent execution
- Shared data should be properly synchronized by the user

## Contributing
This framework is designed to be extensible. You can:
- Create custom node types by inheriting from base classes
- Implement custom flow logic
- Add new execution patterns by combining existing interfaces

## License
[[LICENSE](LICENSE)]
## Dependencies
- Standard C++20 library
- Threading support (, `<async>`) `<future>`
- Type utilities (, `<typeinfo>`) `<any>`

