# C++ Agentic Framework - API Reference

## Table of Contents

1. [Overview](#overview)
2. [Core Components](#core-components)
3. [Data Types](#data-types)
4. [Node Types](#node-types)
5. [Flow Management](#flow-management)
6. [Error Handling](#error-handling)
7. [Concurrency & Thread Safety](#concurrency--thread-safety)
8. [Performance Optimization](#performance-optimization)
9. [Examples](#examples)

## Overview

The C++ Agentic Framework is a modern, thread-safe framework for building data processing pipelines with support for synchronous and asynchronous operations, batch processing, and comprehensive error handling.

### Key Features

- **Type Safety**: Strong compile-time and runtime type checking
- **Thread Safety**: Built-in synchronization for concurrent operations
- **Memory Safety**: RAII-based resource management with smart pointers
- **Error Handling**: Comprehensive error propagation and recovery
- **Performance**: Optimized for high-throughput scenarios
- **Extensibility**: Plugin-based architecture for custom nodes

### Thread Safety Guarantees

- **BaseNode**: All parameter and successor operations are thread-safe
- **AsyncNode**: Concurrent execution protection with atomic state tracking
- **FrameworkData**: Immutable after construction, safe for concurrent read access
- **TypedData**: Thread-safe for read operations, requires external synchronization for writes
- **Result**: Thread-safe for all operations

## Core Components

### FrameworkData

The base class for all data types in the framework, providing polymorphic behavior and type safety.

```cpp
struct FrameworkData {
    virtual ~FrameworkData() = default;
    
    // Pure virtual methods - must be implemented by derived classes
    [[nodiscard]] virtual std::unique_ptr<FrameworkData> clone() const = 0;
    [[nodiscard]] virtual const std::type_info& type() const noexcept = 0;
    
    // Optional serialization support
    [[nodiscard]] virtual std::string serialize() const;
    virtual bool deserialize(std::string_view data);
    
    // Type-safe casting helpers
    template<typename T>
    [[nodiscard]] const T* as() const noexcept;
    
    template<typename T>
    [[nodiscard]] T* as() noexcept;
    
    template<typename T>
    [[nodiscard]] const T& safe_as() const;  // Throws on failure
    
    template<typename T>
    [[nodiscard]] bool is() const noexcept;  // Type checking
};
```

#### Usage Example

```cpp
#include "BaseNode/FrameworkData.h"
#include "Flow/StringData.h"

// Create data
auto data = std::make_shared<StringData>("Hello, World!");

// Type-safe casting
if (auto* str_data = data->as<StringData>()) {
    std::cout << "String value: " << str_data->value << std::endl;
}

// Safe casting with exception on failure
try {
    const auto& str_ref = data->safe_as<StringData>();
    std::cout << "String value: " << str_ref.value << std::endl;
} catch (const std::bad_cast& e) {
    std::cerr << "Cast failed: " << e.what() << std::endl;
}

// Type checking
if (data->is<StringData>()) {
    std::cout << "Data is StringData" << std::endl;
}
```

### BaseNode

The foundation class for all processing nodes, providing parameter management and successor chaining.

```cpp
class BaseNode : public std::enable_shared_from_this<BaseNode> {
public:
    // Parameter management (thread-safe)
    virtual void setParam(const std::map<std::string, std::shared_ptr<FrameworkData>>& params);
    virtual void setParam(std::map<std::string, std::shared_ptr<FrameworkData>>&& params);
    [[nodiscard]] virtual std::shared_ptr<FrameworkData> getParam(std::string_view key) const;
    [[nodiscard]] virtual std::map<std::string, std::shared_ptr<FrameworkData>> getParams() const;
    
    // Safe parameter access with validation
    [[nodiscard]] virtual std::shared_ptr<FrameworkData> getParamSafe(std::string_view key) const;
    virtual void setParamSafe(std::string_view key, std::shared_ptr<FrameworkData> value);
    virtual void validateParams() const;
    
    // Successor management (thread-safe)
    virtual std::shared_ptr<BaseNode> next(std::shared_ptr<BaseNode> node, 
                                          std::string_view action = "default");
    [[nodiscard]] virtual std::map<std::string, std::shared_ptr<BaseNode>> getSuccessor() const;
    
    // Processing pipeline
    virtual std::shared_ptr<FrameworkData> Prep(std::shared_ptr<FrameworkData> shared);
    virtual std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res);
    virtual std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared,
                                               std::shared_ptr<FrameworkData> prep_res,
                                               std::shared_ptr<FrameworkData> exec_res);
    virtual std::shared_ptr<FrameworkData> Run(std::shared_ptr<FrameworkData> shared);
    virtual std::shared_ptr<FrameworkData> Execute(std::shared_ptr<FrameworkData> shared);
    
    // Operator overloads for chaining
    std::shared_ptr<BaseNode> operator>>(const std::shared_ptr<BaseNode>& other);
    ConditionalTransition operator-(std::string_view action);
};
```

#### Usage Example

```cpp
#include "BaseNode/BaseNode.h"
#include "Flow/StringData.h"

class MyProcessingNode : public BaseNode {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // Get configuration parameter
        auto config = getParamSafe("config");
        if (!config) {
            throw std::runtime_error("Missing required parameter: config");
        }
        
        // Process the data
        if (auto* input = prep_res->as<StringData>()) {
            auto result = std::make_shared<StringData>(
                "Processed: " + input->value
            );
            return result;
        }
        
        return prep_res;
    }
};

// Usage
auto node = std::make_shared<MyProcessingNode>();

// Set parameters
std::map<std::string, std::shared_ptr<FrameworkData>> params;
params["config"] = std::make_shared<StringData>("production");
node->setParam(std::move(params));

// Process data
auto input = std::make_shared<StringData>("test data");
auto result = node->Execute(input);

// Chain nodes
auto node2 = std::make_shared<MyProcessingNode>();
auto chained = node >> node2;  // node2 will process node's output
```

## Data Types

### TypedData<T>

A type-safe container with validation and constraint checking capabilities.

```cpp
template<typename T>
class TypedData : public FrameworkData {
public:
    using value_type = T;
    
    // Constructors
    explicit TypedData(T data);
    TypedData(T data, std::vector<Constraint<T>> constraints);
    
    // Value access
    [[nodiscard]] const T& get() const noexcept;
    [[nodiscard]] T& get() noexcept;
    void set(T new_value);  // Validates against constraints
    
    // Constraint management
    void add_constraint(Constraint<T> constraint);
    void clear_constraints();
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] std::vector<std::string> get_validation_errors() const;
    
    // Serialization (for supported types)
    std::string serialize() const override;
    bool deserialize(std::string_view data) override;
};
```

#### Usage Example

```cpp
#include "BaseNode/TypedData.h"

// Create typed data with constraints
auto age_data = std::make_unique<TypedData<int>>(
    25, 
    {Constraints<int>::range(0, 150)}
);

// Access value
std::cout << "Age: " << age_data->get() << std::endl;

// Validate and set new value
try {
    age_data->set(30);  // Valid
    age_data->set(-5);  // Throws due to constraint violation
} catch (const std::invalid_argument& e) {
    std::cerr << "Validation error: " << e.what() << std::endl;
}

// Check validation status
if (!age_data->is_valid()) {
    auto errors = age_data->get_validation_errors();
    for (const auto& error : errors) {
        std::cerr << "Error: " << error << std::endl;
    }
}

// Common type aliases
using IntData = TypedData<int>;
using StringTypedData = TypedData<std::string>;
using DoubleData = TypedData<double>;
```

### Result<T>

A type for operations that can fail, providing monadic operations for error handling.

```cpp
template<typename T>
class Result {
public:
    // Constructors
    Result(T value);
    Result(FrameworkError error);
    
    // Status checking
    [[nodiscard]] bool has_value() const noexcept;
    [[nodiscard]] bool has_error() const noexcept;
    explicit operator bool() const noexcept;
    
    // Value access
    [[nodiscard]] const T& value() const &;  // Throws if error
    [[nodiscard]] T&& value() &&;
    [[nodiscard]] const FrameworkError& error() const &;
    
    template<typename U>
    [[nodiscard]] T value_or(U&& default_value) const &;
    
    // Monadic operations
    template<typename F>
    [[nodiscard]] auto map(F&& func) const & -> Result<std::invoke_result_t<F, const T&>>;
    
    template<typename F>
    [[nodiscard]] auto flat_map(F&& func) const & -> std::invoke_result_t<F, const T&>;
    
    template<typename F>
    [[nodiscard]] Result map_error(F&& func) const &;
};
```

#### Usage Example

```cpp
#include "BaseNode/Result.h"
#include "BaseNode/FrameworkError.h"

using namespace framework;

// Function that can fail
Result<int> divide(int a, int b) {
    if (b == 0) {
        return FrameworkError::invalid_argument("Division by zero");
    }
    return a / b;
}

// Usage with error handling
auto result = divide(10, 2);
if (result.has_value()) {
    std::cout << "Result: " << result.value() << std::endl;
} else {
    std::cerr << "Error: " << result.error().what() << std::endl;
}

// Monadic operations
auto final_result = divide(20, 4)
    .map([](int x) { return x * 2; })          // Transform value
    .flat_map([](int x) { return divide(x, 2); })  // Chain operations
    .map_error([](const FrameworkError& e) {        // Transform error
        return FrameworkError::runtime_error("Calculation failed: " + std::string(e.what()));
    });

// Get value with default
int value = divide(10, 0).value_or(-1);  // Returns -1 on error
```

## Node Types

### Node

Synchronous processing node with retry capabilities.

```cpp
class Node : virtual public BaseNode {
public:
    std::atomic<int> max_retries;
    std::atomic<int> wait;  // Wait time between retries (ms)
    
    explicit Node(int max_retries = 1, int wait = 0);
    
    // Fallback handling for failed operations
    virtual std::shared_ptr<FrameworkData> exec_fallback(
        const std::shared_ptr<FrameworkData>& prep_res, 
        std::exception_ptr eptr) const;
};
```

#### Usage Example

```cpp
#include "Node/Node.h"

class RetryableNode : public Node {
public:
    RetryableNode() : Node(3, 1000) {}  // 3 retries, 1 second wait
    
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // Simulate operation that might fail
        static int attempt = 0;
        if (++attempt < 3) {
            throw std::runtime_error("Temporary failure");
        }
        
        return std::make_shared<StringData>("Success after retries");
    }
    
    std::shared_ptr<FrameworkData> exec_fallback(
        const std::shared_ptr<FrameworkData>& prep_res,
        std::exception_ptr eptr) const override {
        
        try {
            std::rethrow_exception(eptr);
        } catch (const std::exception& e) {
            std::cerr << "All retries failed: " << e.what() << std::endl;
            return std::make_shared<StringData>("Fallback result");
        }
    }
};
```

### AsyncNode

Asynchronous processing node with timeout and cancellation support.

```cpp
class AsyncNode : virtual public Node {
public:
    // Async processing pipeline
    virtual std::future<std::shared_ptr<FrameworkData>> PrepAsync(std::shared_ptr<FrameworkData> shared);
    virtual std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> prep_res);
    virtual std::future<std::shared_ptr<FrameworkData>> PostAsync(
        std::shared_ptr<FrameworkData> shared,
        std::shared_ptr<FrameworkData> prep_res,
        std::shared_ptr<FrameworkData> exec_res);
    
    // Main async execution
    virtual std::future<std::shared_ptr<FrameworkData>> RunAsync(std::shared_ptr<FrameworkData> shared);
    
    // Execution with timeout
    virtual std::future<std::shared_ptr<FrameworkData>> RunAsyncWithTimeout(
        std::shared_ptr<FrameworkData> shared, 
        std::chrono::milliseconds timeout);
    
    // Execution with cancellation
    virtual std::future<std::shared_ptr<FrameworkData>> RunAsyncWithCancellation(
        std::shared_ptr<FrameworkData> shared, 
        const CancellationToken& token);
    
    // Check execution status
    [[nodiscard]] bool is_executing() const noexcept;
};
```

#### Usage Example

```cpp
#include "AsyncNode/AsyncNode.h"
#include "BaseNode/CancellationToken.h"

class AsyncProcessingNode : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [prep_res]() {
            // Simulate long-running operation
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            if (auto* input = prep_res->as<StringData>()) {
                return std::static_pointer_cast<FrameworkData>(
                    std::make_shared<StringData>("Async processed: " + input->value)
                );
            }
            return prep_res;
        });
    }
};

// Usage
auto async_node = std::make_shared<AsyncProcessingNode>();
auto input = std::make_shared<StringData>("test data");

// Basic async execution
auto future = async_node->RunAsync(input);
auto result = future.get();  // Blocks until completion

// Execution with timeout
try {
    auto timeout_future = async_node->RunAsyncWithTimeout(
        input, 
        std::chrono::milliseconds(5000)
    );
    auto result = timeout_future.get();
} catch (const std::runtime_error& e) {
    std::cerr << "Operation timed out: " << e.what() << std::endl;
}

// Execution with cancellation
CancellationToken token;
auto cancel_future = async_node->RunAsyncWithCancellation(input, token);

// Cancel after 1 second
std::this_thread::sleep_for(std::chrono::seconds(1));
token.cancel();

try {
    auto result = cancel_future.get();
} catch (const std::runtime_error& e) {
    std::cerr << "Operation cancelled: " << e.what() << std::endl;
}
```

### BatchNode

Node for processing collections of data items.

```cpp
class BatchNode : virtual public Node {
public:
    using Node::Node;
    
protected:
    // Override to implement batch processing logic
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override;
};
```

#### Usage Example

```cpp
#include "BatchNode/BatchNode.h"
#include "BatchNode/VectorData.h"

class BatchProcessingNode : public BatchNode {
public:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData for batch processing");
        }
        
        auto result = std::make_shared<VectorData>();
        
        // Process each item in the batch
        for (const auto& item : batch->data) {
            if (auto* str_item = item->as<StringData>()) {
                auto processed = std::make_shared<StringData>(
                    "Batch processed: " + str_item->value
                );
                result->data.push_back(processed);
            }
        }
        
        return result;
    }
};

// Usage
auto batch_node = std::make_shared<BatchProcessingNode>();

// Create batch data
auto batch = std::make_shared<VectorData>();
batch->data.push_back(std::make_shared<StringData>("item1"));
batch->data.push_back(std::make_shared<StringData>("item2"));
batch->data.push_back(std::make_shared<StringData>("item3"));

// Process batch
auto result = batch_node->Execute(batch);
```

### AsyncBatchNode

Combines asynchronous processing with batch operations for non-blocking batch processing.

```cpp
class AsyncBatchNode : public AsyncNode {
private:
    // Override to implement async batch processing logic
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override;
};
```

#### Usage Example

```cpp
#include "AsyncBatchNode/AsyncBatchNode.h"
#include "BatchNode/VectorData.h"

class AsyncBatchProcessingNode : public AsyncBatchNode {
private:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData for async batch processing");
        }
        
        auto result = std::make_shared<VectorData>();
        result->data.reserve(batch->data.size());
        
        // Process each item asynchronously (in sequence within this method)
        for (const auto& item : batch->data) {
            if (auto* str_item = item->as<StringData>()) {
                // Simulate async processing
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
                auto processed = std::make_shared<StringData>(
                    "Async batch processed: " + str_item->value
                );
                result->data.push_back(processed);
            }
        }
        
        return result;
    }
};

// Usage
auto async_batch_node = std::make_shared<AsyncBatchProcessingNode>();
auto batch = std::make_shared<VectorData>();
batch->data.push_back(std::make_shared<StringData>("async_item1"));
batch->data.push_back(std::make_shared<StringData>("async_item2"));

// Process asynchronously
auto future = async_batch_node->RunAsync(batch);
auto result = future.get();  // Non-blocking batch processing
```

### AsyncParallelBatchNode

Provides parallel processing of batch items with full asynchronous execution.

```cpp
class AsyncParallelBatchNode : public AsyncNode {
private:
    // Override to implement parallel async batch processing
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override;
};
```

#### Usage Example

```cpp
#include "AsyncParallelBatchNode/AsyncParallelBatchNode.h"
#include "BatchNode/VectorData.h"

class ParallelBatchProcessingNode : public AsyncParallelBatchNode {
private:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData for parallel batch processing");
        }
        
        // Process items in parallel using futures
        std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
        futures.reserve(batch->data.size());
        
        // Launch parallel processing tasks
        for (const auto& item : batch->data) {
            futures.emplace_back(
                std::async(std::launch::async, [item]() -> std::shared_ptr<FrameworkData> {
                    if (auto* str_item = item->as<StringData>()) {
                        // Simulate CPU-intensive work
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        
                        return std::make_shared<StringData>(
                            "Parallel processed: " + str_item->value
                        );
                    }
                    return item;
                })
            );
        }
        
        // Collect results
        auto result = std::make_shared<VectorData>();
        result->data.reserve(futures.size());
        
        for (auto& future : futures) {
            result->data.push_back(future.get());
        }
        
        return result;
    }
};

// Usage
auto parallel_batch_node = std::make_shared<ParallelBatchProcessingNode>();
auto large_batch = std::make_shared<VectorData>();

// Create large batch for parallel processing
for (int i = 0; i < 100; ++i) {
    large_batch->data.push_back(
        std::make_shared<StringData>("parallel_item_" + std::to_string(i))
    );
}

// Process in parallel asynchronously
auto future = parallel_batch_node->RunAsync(large_batch);
auto result = future.get();  // All items processed in parallel
```

## Flow Management

### Flow

Orchestrates execution through a sequence of connected nodes.

```cpp
class Flow : virtual public BaseNode {
public:
    explicit Flow(std::shared_ptr<BaseNode> start_node = nullptr);
    
    // Set the starting node
    virtual std::shared_ptr<BaseNode> Start(std::shared_ptr<BaseNode> start);
    
    // Navigate through the flow
    [[nodiscard]] static std::shared_ptr<BaseNode> GetNextNode(
        const std::shared_ptr<BaseNode>& curr, 
        std::string_view action);
    
    // Execute the flow
    virtual std::shared_ptr<FrameworkData> orch(
        std::shared_ptr<FrameworkData> shared, 
        const std::map<std::string, std::shared_ptr<FrameworkData>>& params = {});
};
```

#### Usage Example

```cpp
#include "Flow/Flow.h"

// Create nodes
auto node1 = std::make_shared<MyProcessingNode>();
auto node2 = std::make_shared<MyProcessingNode>();
auto node3 = std::make_shared<MyProcessingNode>();

// Set up flow
node1->next(node2, "success");
node1->next(node3, "error");
node2->next(node3, "default");

// Create and execute flow
auto flow = std::make_shared<Flow>(node1);
auto input = std::make_shared<StringData>("flow input");

std::map<std::string, std::shared_ptr<FrameworkData>> flow_params;
flow_params["global_config"] = std::make_shared<StringData>("production");

auto result = flow->orch(input, flow_params);
```

### AsyncFlow

Combines Flow orchestration with asynchronous execution capabilities.

```cpp
class AsyncFlow : virtual public Flow, virtual public AsyncNode {
protected:
    std::mutex shared_mutex;  // Thread-safe flow execution
    
    // Async flow orchestration
    std::future<std::shared_ptr<FrameworkData>> orchAsync(
        std::shared_ptr<FrameworkData> shared,
        const std::map<std::string, std::shared_ptr<FrameworkData>>& params = {});
    
public:
    using Flow::Flow;
    using AsyncNode::AsyncNode;
    
    // Async post-processing
    std::future<std::shared_ptr<FrameworkData>> PostAsync(
        std::shared_ptr<FrameworkData> shared,
        std::shared_ptr<FrameworkData> prep_res,
        std::shared_ptr<FrameworkData> exec_res) override;
};
```

#### Usage Example

```cpp
#include "AsyncFlow/AsyncFlow.h"

class AsyncProcessingFlow : public AsyncFlow {
public:
    AsyncProcessingFlow(std::shared_ptr<BaseNode> start_node) : AsyncFlow(start_node) {}
};

// Create async nodes
auto async_node1 = std::make_shared<AsyncProcessingNode>();
auto async_node2 = std::make_shared<AsyncValidationNode>();
auto async_node3 = std::make_shared<AsyncOutputNode>();

// Set up async flow
async_node1->next(async_node2, "validated");
async_node2->next(async_node3, "success");

// Create and execute async flow
auto async_flow = std::make_shared<AsyncProcessingFlow>(async_node1);
auto input = std::make_shared<StringData>("async flow input");

// Execute asynchronously
auto future = async_flow->RunAsync(input);

// Do other work while flow executes...
std::cout << "Flow executing in background..." << std::endl;

// Get result when ready
auto result = future.get();
```

### BatchFlow

Extends Flow to handle batch data processing through connected nodes.

```cpp
class BatchFlow : virtual public Flow {
public:
    using Flow::Flow;
    
private:
    // Override to handle batch-specific flow execution
    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override;
    
    // Batch-specific post-processing
    std::shared_ptr<FrameworkData> Post(
        std::shared_ptr<FrameworkData> shared,
        std::shared_ptr<FrameworkData> prep_res,
        std::shared_ptr<FrameworkData> exec_res) override;
};
```

#### Usage Example

```cpp
#include "BatchFlow/BatchFlow.h"
#include "BatchNode/VectorData.h"

// Create batch processing nodes
auto batch_validator = std::make_shared<BatchValidationNode>();
auto batch_processor = std::make_shared<BatchProcessingNode>();
auto batch_aggregator = std::make_shared<BatchAggregatorNode>();

// Set up batch flow
batch_validator->next(batch_processor, "valid");
batch_processor->next(batch_aggregator, "processed");

// Create batch flow
auto batch_flow = std::make_shared<BatchFlow>(batch_validator);

// Create batch input
auto batch_input = std::make_shared<VectorData>();
for (int i = 0; i < 1000; ++i) {
    batch_input->data.push_back(
        std::make_shared<StringData>("batch_item_" + std::to_string(i))
    );
}

// Process entire batch through flow
auto batch_result = batch_flow->Execute(batch_input);

if (auto* result_batch = batch_result->as<VectorData>()) {
    std::cout << "Processed " << result_batch->data.size() << " items" << std::endl;
}
```

### AsyncBatchFlow

Combines AsyncFlow and BatchFlow for asynchronous batch processing workflows.

```cpp
class AsyncBatchFlow : public AsyncFlow {
public:
    using AsyncFlow::AsyncFlow;
    
    // Batch-specific post-processing
    std::shared_ptr<FrameworkData> Post(
        std::shared_ptr<FrameworkData> shared,
        std::shared_ptr<FrameworkData> prep_res,
        std::shared_ptr<FrameworkData> exec_res) override;
        
private:
    // Async batch flow execution
    std::future<std::shared_ptr<FrameworkData>> runAsync(std::shared_ptr<FrameworkData> shared) override;
    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override;
};
```

#### Usage Example

```cpp
#include "AsyncBatchFlow/AsyncBatchFlow.h"

// Create async batch processing nodes
auto async_batch_loader = std::make_shared<AsyncBatchLoaderNode>();
auto async_batch_processor = std::make_shared<AsyncBatchProcessorNode>();
auto async_batch_saver = std::make_shared<AsyncBatchSaverNode>();

// Set up async batch flow
async_batch_loader->next(async_batch_processor, "loaded");
async_batch_processor->next(async_batch_saver, "processed");

// Create async batch flow
auto async_batch_flow = std::make_shared<AsyncBatchFlow>(async_batch_loader);

// Create large batch for async processing
auto large_batch = std::make_shared<VectorData>();
for (int i = 0; i < 10000; ++i) {
    large_batch->data.push_back(
        std::make_shared<StringData>("async_batch_item_" + std::to_string(i))
    );
}

// Process asynchronously
auto future = async_batch_flow->RunAsync(large_batch);

// Monitor progress or do other work
std::cout << "Large batch processing in background..." << std::endl;

// Get result when complete
auto result = future.get();
```

### AsyncParallelBatchFlow

The most advanced flow type, combining async execution, batch processing, and parallel execution.

```cpp
class AsyncParallelBatchFlow : virtual public AsyncFlow {
public:
    using AsyncFlow::AsyncFlow;
    
    // Parallel batch post-processing
    std::shared_ptr<FrameworkData> Post(
        std::shared_ptr<FrameworkData> shared,
        std::shared_ptr<FrameworkData> prep_res,
        std::shared_ptr<FrameworkData> exec_res) override;
        
private:
    // Parallel async batch flow execution
    std::future<std::shared_ptr<FrameworkData>> runAsync(std::shared_ptr<FrameworkData> shared) override;
    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override;
};
```

#### Usage Example

```cpp
#include "AsyncParallelBatchFlow/AsyncParallelBatchFlow.h"

// Create parallel batch processing nodes
auto parallel_batch_splitter = std::make_shared<ParallelBatchSplitterNode>();
auto parallel_batch_processor = std::make_shared<AsyncParallelBatchNode>();
auto parallel_batch_merger = std::make_shared<ParallelBatchMergerNode>();

// Set up parallel batch flow
parallel_batch_splitter->next(parallel_batch_processor, "split");
parallel_batch_processor->next(parallel_batch_merger, "processed");

// Create parallel batch flow
auto parallel_flow = std::make_shared<AsyncParallelBatchFlow>(parallel_batch_splitter);

// Create massive batch for parallel processing
auto massive_batch = std::make_shared<VectorData>();
for (int i = 0; i < 100000; ++i) {
    massive_batch->data.push_back(
        std::make_shared<StringData>("parallel_item_" + std::to_string(i))
    );
}

// Process with maximum parallelism
auto start_time = std::chrono::high_resolution_clock::now();
auto future = parallel_flow->RunAsync(massive_batch);

// Optional: Add timeout for very large batches
try {
    auto result = parallel_flow->RunAsyncWithTimeout(
        massive_batch, 
        std::chrono::minutes(5)
    ).get();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time
    );
    
    std::cout << "Processed 100,000 items in " << duration.count() 
              << " seconds using parallel batch flow" << std::endl;
              
} catch (const std::runtime_error& e) {
    std::cout << "Parallel batch processing timed out: " << e.what() << std::endl;
}
```

## Flow Type Comparison

| Flow Type | Sync | Async | Batch | Parallel | Best Use Case |
|-----------|------|-------|-------|----------|---------------|
| **Flow** | ✓ | ✗ | ✗ | ✗ | Simple sequential processing |
| **AsyncFlow** | ✓ | ✓ | ✗ | ✗ | Non-blocking workflows |
| **BatchFlow** | ✓ | ✗ | ✓ | ✗ | Large dataset processing |
| **AsyncBatchFlow** | ✓ | ✓ | ✓ | ✗ | Non-blocking batch processing |
| **AsyncParallelBatchFlow** | ✓ | ✓ | ✓ | ✓ | High-throughput parallel processing |

## Error Handling

### FrameworkError

Structured error information with context and error codes.

```cpp
class FrameworkError : public std::exception {
public:
    // Factory methods for common error types
    static FrameworkError invalid_argument(std::string message, std::string context = "");
    static FrameworkError runtime_error(std::string message, std::string context = "");
    static FrameworkError logic_error(std::string message, std::string context = "");
    static FrameworkError timeout_error(std::string message, std::string context = "");
    
    // Access error information
    const char* what() const noexcept override;
    const std::string& message() const noexcept;
    const std::string& context() const noexcept;
    const std::error_code& code() const noexcept;
    ErrorCategory category() const noexcept;
};
```

#### Usage Example

```cpp
#include "BaseNode/FrameworkError.h"

// Throwing structured errors
void validate_input(const std::string& input) {
    if (input.empty()) {
        throw FrameworkError::invalid_argument(
            "Input cannot be empty", 
            "validate_input"
        );
    }
    
    if (input.length() > 1000) {
        throw FrameworkError::invalid_argument(
            "Input too long (max 1000 characters)", 
            "validate_input"
        );
    }
}

// Handling errors
try {
    validate_input("");
} catch (const FrameworkError& e) {
    std::cerr << "Error in " << e.context() << ": " << e.what() << std::endl;
    std::cerr << "Error code: " << e.code().value() << std::endl;
    std::cerr << "Category: " << static_cast<int>(e.category()) << std::endl;
}
```

## Concurrency & Thread Safety

### Thread Safety Guidelines

1. **BaseNode Operations**: All parameter and successor operations are thread-safe
2. **AsyncNode Execution**: Prevents concurrent execution of the same node
3. **Data Sharing**: Use `std::shared_ptr` for safe data sharing between threads
4. **Immutable Data**: Prefer immutable data structures for concurrent access

### CancellationToken

Provides cooperative cancellation for long-running operations.

```cpp
class CancellationToken {
public:
    CancellationToken();
    
    // Check if cancellation was requested
    [[nodiscard]] bool is_cancelled() const noexcept;
    
    // Request cancellation
    void cancel() noexcept;
    
    // Throw if cancelled
    void throw_if_cancelled() const;
    
    // Wait for cancellation or timeout
    template<typename Rep, typename Period>
    bool wait_for_cancellation(const std::chrono::duration<Rep, Period>& timeout) const;
};
```

#### Usage Example

```cpp
#include "BaseNode/CancellationToken.h"

void long_running_operation(const CancellationToken& token) {
    for (int i = 0; i < 1000; ++i) {
        // Check for cancellation periodically
        token.throw_if_cancelled();
        
        // Do some work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Alternative: non-throwing check
        if (token.is_cancelled()) {
            std::cout << "Operation cancelled gracefully" << std::endl;
            return;
        }
    }
}

// Usage
CancellationToken token;

// Start operation in background
auto future = std::async(std::launch::async, [&token]() {
    try {
        long_running_operation(token);
        return "Completed";
    } catch (const std::runtime_error&) {
        return "Cancelled";
    }
});

// Cancel after 2 seconds
std::this_thread::sleep_for(std::chrono::seconds(2));
token.cancel();

std::cout << "Result: " << future.get() << std::endl;
```

## Performance Optimization

### Memory Management Best Practices

1. **Use Smart Pointers**: Always use `std::shared_ptr` and `std::unique_ptr`
2. **Avoid Copies**: Use move semantics and perfect forwarding
3. **Reserve Containers**: Pre-allocate container capacity when size is known
4. **Object Pooling**: Reuse objects for frequently allocated types

### Batch Processing Optimization

```cpp
#include "BaseNode/OptimizedBatchProcessor.h"

class OptimizedBatchProcessor {
public:
    template<typename InputRange, typename Processor>
    auto process_batch(InputRange&& input, Processor&& processor);
    
    // Configure thread pool size
    void set_thread_count(size_t count);
    
    // Enable SIMD optimizations (where applicable)
    void enable_simd(bool enable = true);
};
```

#### Usage Example

```cpp
// Create optimized batch processor
OptimizedBatchProcessor processor;
processor.set_thread_count(std::thread::hardware_concurrency());
processor.enable_simd(true);

// Process large batch efficiently
std::vector<int> large_dataset(1000000);
std::iota(large_dataset.begin(), large_dataset.end(), 1);

auto results = processor.process_batch(
    large_dataset,
    [](int value) { return value * value; }  // Square each value
);

std::cout << "Processed " << results.size() << " items" << std::endl;
```

### Memory Tracking

```cpp
#include "BaseNode/MemoryTracker.h"

// Track memory usage
MemoryTracker tracker;
tracker.start_tracking();

// Your operations here
auto data = std::make_shared<StringData>("large data");
// ... more operations

auto usage = tracker.get_current_usage();
std::cout << "Memory used: " << usage.bytes_allocated << " bytes" << std::endl;
std::cout << "Peak usage: " << usage.peak_bytes << " bytes" << std::endl;
```

## Examples

### Complete Processing Pipeline

```cpp
#include "Flow/Flow.h"
#include "Node/Node.h"
#include "AsyncNode/AsyncNode.h"
#include "BatchNode/BatchNode.h"

// Custom validation node
class ValidationNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* str_data = prep_res->as<StringData>()) {
            if (str_data->value.empty()) {
                throw FrameworkError::invalid_argument("Empty input not allowed");
            }
        }
        return prep_res;
    }
};

// Custom transformation node
class TransformNode : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [prep_res]() {
            if (auto* str_data = prep_res->as<StringData>()) {
                // Simulate async transformation
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                auto result = std::make_shared<StringData>(
                    "TRANSFORMED: " + str_data->value
                );
                return std::static_pointer_cast<FrameworkData>(result);
            }
            return prep_res;
        });
    }
};

// Custom output node
class OutputNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* str_data = prep_res->as<StringData>()) {
            std::cout << "Final result: " << str_data->value << std::endl;
        }
        return prep_res;
    }
};

int main() {
    try {
        // Create nodes
        auto validator = std::make_shared<ValidationNode>();
        auto transformer = std::make_shared<TransformNode>();
        auto output = std::make_shared<OutputNode>();
        
        // Chain nodes
        validator->next(transformer);
        transformer->next(output);
        
        // Create flow
        auto flow = std::make_shared<Flow>(validator);
        
        // Process data
        auto input = std::make_shared<StringData>("Hello, Framework!");
        auto result = flow->orch(input);
        
        std::cout << "Pipeline completed successfully" << std::endl;
        
    } catch (const FrameworkError& e) {
        std::cerr << "Framework error: " << e.what() << std::endl;
        std::cerr << "Context: " << e.context() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Standard error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

### Error Handling with Result Types

```cpp
#include "BaseNode/Result.h"

using namespace framework;

// Function that returns Result
Result<std::shared_ptr<FrameworkData>> safe_process(
    std::shared_ptr<FrameworkData> input) {
    
    if (!input) {
        return FrameworkError::invalid_argument("Input cannot be null");
    }
    
    try {
        // Process the input
        if (auto* str_data = input->as<StringData>()) {
            auto result = std::make_shared<StringData>(
                "Safely processed: " + str_data->value
            );
            return std::static_pointer_cast<FrameworkData>(result);
        }
        
        return FrameworkError::invalid_argument("Unsupported data type");
        
    } catch (const std::exception& e) {
        return FrameworkError::runtime_error(e.what());
    }
}

int main() {
    auto input = std::make_shared<StringData>("test data");
    
    // Chain operations with automatic error propagation
    auto final_result = safe_process(input)
        .flat_map([](auto data) { return safe_process(data); })  // Chain another operation
        .map([](auto data) {  // Transform successful result
            std::cout << "Processing completed successfully" << std::endl;
            return data;
        })
        .map_error([](const FrameworkError& e) {  // Handle errors
            std::cerr << "Processing failed: " << e.what() << std::endl;
            return e;
        });
    
    // Check final result
    if (final_result.has_value()) {
        std::cout << "Success!" << std::endl;
    } else {
        std::cout << "Failed: " << final_result.error().what() << std::endl;
    }
    
    return 0;
}
```

This API reference provides comprehensive documentation for all public interfaces in the C++ Agentic Framework, with clear usage examples and thread safety guarantees.