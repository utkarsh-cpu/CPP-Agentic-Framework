# Getting Started with C++ Agentic Framework

## Table of Contents

1. [Installation](#installation)
2. [Basic Concepts](#basic-concepts)
3. [Your First Node](#your-first-node)
4. [Common Usage Patterns](#common-usage-patterns)
5. [Working with Data](#working-with-data)
6. [Error Handling](#error-handling)
7. [Async Operations](#async-operations)
8. [Best Practices](#best-practices)

## Installation

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.15 or higher
- Optional: Sanitizers for development (AddressSanitizer, ThreadSanitizer)

### Building the Framework

```bash
# Clone the repository
git clone <repository-url>
cd agentic-framework

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --parallel

# Run tests (optional)
ctest --parallel
```

### CMake Integration

Add to your `CMakeLists.txt`:

```cmake
# Find the framework
find_package(AgenticFramework REQUIRED)

# Link to your target
target_link_libraries(your_target 
    PRIVATE AgenticFramework::Core
)

# Enable C++20
target_compile_features(your_target PRIVATE cxx_std_20)
```

## Basic Concepts

### Core Components

The framework is built around several key concepts:

1. **FrameworkData**: Base class for all data types
2. **BaseNode**: Base class for all processing nodes
3. **Flow**: Orchestrates execution through connected nodes
4. **Result**: Type-safe error handling
5. **TypedData**: Type-safe data containers with validation

### Component Hierarchy

The framework provides a rich hierarchy of components for different use cases:

#### Node Types
- **Node**: Basic synchronous processing with retry capabilities
- **AsyncNode**: Asynchronous processing with futures, timeouts, and cancellation
- **BatchNode**: Synchronous batch processing for collections of data
- **AsyncBatchNode**: Asynchronous batch processing (non-blocking)
- **AsyncParallelBatchNode**: Parallel batch processing for maximum throughput

#### Flow Types
- **Flow**: Basic workflow orchestration with conditional routing
- **AsyncFlow**: Asynchronous workflow execution (non-blocking)
- **BatchFlow**: Workflow for batch data processing
- **AsyncBatchFlow**: Asynchronous batch workflow processing
- **AsyncParallelBatchFlow**: Maximum performance parallel batch workflows

#### Capability Matrix

| Component | Sync | Async | Batch | Parallel | Best For |
|-----------|------|-------|-------|----------|----------|
| **Node** | ✓ | ✗ | ✗ | ✗ | Simple processing |
| **AsyncNode** | ✓ | ✓ | ✗ | ✗ | Non-blocking operations |
| **BatchNode** | ✓ | ✗ | ✓ | ✗ | Large datasets |
| **AsyncBatchNode** | ✓ | ✓ | ✓ | ✗ | Non-blocking batches |
| **AsyncParallelBatchNode** | ✓ | ✓ | ✓ | ✓ | High-throughput processing |
| **Flow** | ✓ | ✗ | ✗ | ✗ | Simple workflows |
| **AsyncFlow** | ✓ | ✓ | ✗ | ✗ | Non-blocking workflows |
| **BatchFlow** | ✓ | ✗ | ✓ | ✗ | Batch workflows |
| **AsyncBatchFlow** | ✓ | ✓ | ✓ | ✗ | Non-blocking batch workflows |
| **AsyncParallelBatchFlow** | ✓ | ✓ | ✓ | ✓ | Maximum performance workflows |

### Data Flow Architecture

```
Input Data → Node 1 → Node 2 → Node 3 → Output Data
              ↓        ↓        ↓
           Parameters Parameters Parameters
```

Each node in the pipeline:
- Receives input data
- Processes it according to its logic
- Passes the result to the next node
- Can access configuration parameters
- Handles errors gracefully

## Your First Node

Let's create a simple text processing node:

### Step 1: Create a Custom Node

```cpp
#include "Node/Node.h"
#include "Flow/StringData.h"
#include <algorithm>
#include <cctype>

class TextProcessorNode : public Node {
public:
    // Constructor with retry configuration
    TextProcessorNode() : Node(3, 1000) {  // 3 retries, 1 second wait
        // Set default parameters
        std::map<std::string, std::shared_ptr<FrameworkData>> default_params;
        default_params["operation"] = std::make_shared<StringData>("uppercase");
        setParam(std::move(default_params));
    }
    
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // Get the operation parameter
        auto operation_param = getParam("operation");
        std::string operation = "uppercase";  // default
        
        if (auto* op_data = operation_param->as<StringData>()) {
            operation = op_data->value;
        }
        
        // Process the input
        if (auto* input = prep_res->as<StringData>()) {
            std::string result = input->value;
            
            if (operation == "uppercase") {
                std::transform(result.begin(), result.end(), result.begin(), ::toupper);
            } else if (operation == "lowercase") {
                std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            } else if (operation == "reverse") {
                std::reverse(result.begin(), result.end());
            } else {
                throw std::invalid_argument("Unknown operation: " + operation);
            }
            
            return std::make_shared<StringData>(result);
        }
        
        throw std::invalid_argument("Expected StringData input");
    }
    
    // Optional: Custom fallback for failed operations
    std::shared_ptr<FrameworkData> exec_fallback(
        const std::shared_ptr<FrameworkData>& prep_res,
        std::exception_ptr eptr) const override {
        
        try {
            std::rethrow_exception(eptr);
        } catch (const std::exception& e) {
            std::cerr << "TextProcessor failed: " << e.what() << std::endl;
            // Return original input as fallback
            return prep_res;
        }
    }
};
```

### Step 2: Use Your Node

```cpp
#include "TextProcessorNode.h"

int main() {
    try {
        // Create the node
        auto processor = std::make_shared<TextProcessorNode>();
        
        // Configure the operation
        std::map<std::string, std::shared_ptr<FrameworkData>> params;
        params["operation"] = std::make_shared<StringData>("uppercase");
        processor->setParam(std::move(params));
        
        // Create input data
        auto input = std::make_shared<StringData>("hello world");
        
        // Process the data
        auto result = processor->Execute(input);
        
        // Extract the result
        if (auto* output = result->as<StringData>()) {
            std::cout << "Result: " << output->value << std::endl;  // "HELLO WORLD"
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

## Common Usage Patterns

### Pattern 1: Sequential Processing Chain

```cpp
#include "Flow/Flow.h"

// Create multiple processing nodes
auto validator = std::make_shared<ValidationNode>();
auto transformer = std::make_shared<TextProcessorNode>();
auto logger = std::make_shared<LoggingNode>();

// Chain them together
validator->next(transformer);
transformer->next(logger);

// Create a flow
auto flow = std::make_shared<Flow>(validator);

// Process data through the entire chain
auto input = std::make_shared<StringData>("input data");
auto result = flow->orch(input);
```

### Pattern 2: Conditional Branching

```cpp
// Create nodes for different paths
auto validator = std::make_shared<ValidationNode>();
auto success_processor = std::make_shared<SuccessNode>();
auto error_handler = std::make_shared<ErrorHandlerNode>();
auto final_node = std::make_shared<FinalNode>();

// Set up conditional routing
validator->next(success_processor, "valid");
validator->next(error_handler, "invalid");

// Both paths converge at the final node
success_processor->next(final_node);
error_handler->next(final_node);

// The validator node determines the path based on its logic
class ValidationNode : public Node {
public:
    std::shared_ptr<FrameworkData> Post(
        std::shared_ptr<FrameworkData> shared,
        std::shared_ptr<FrameworkData> prep_res,
        std::shared_ptr<FrameworkData> exec_res) override {
        
        // Determine next action based on validation result
        if (auto* result = exec_res->as<StringData>()) {
            if (result->value == "valid") {
                // Flow will automatically route to "valid" successor
                return exec_res;
            }
        }
        
        // Default routing or error case
        return exec_res;
    }
};
```

### Pattern 3: Parallel Processing with Async Nodes

```cpp
#include "AsyncNode/AsyncNode.h"

class ParallelProcessorNode : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [prep_res]() {
            // CPU-intensive or I/O operation
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            if (auto* input = prep_res->as<StringData>()) {
                auto result = std::make_shared<StringData>(
                    "Processed in parallel: " + input->value
                );
                return std::static_pointer_cast<FrameworkData>(result);
            }
            return prep_res;
        });
    }
};

// Usage
auto async_node = std::make_shared<ParallelProcessorNode>();
auto input = std::make_shared<StringData>("data");

// Start async processing
auto future = async_node->RunAsync(input);

// Do other work while processing...
std::cout << "Processing in background..." << std::endl;

// Get the result when ready
auto result = future.get();
```

### Pattern 4: Batch Processing

```cpp
#include "BatchNode/BatchNode.h"
#include "BatchNode/VectorData.h"

class BatchTextProcessor : public BatchNode {
public:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData for batch processing");
        }
        
        auto result = std::make_shared<VectorData>();
        result->data.reserve(batch->data.size());  // Optimize memory allocation
        
        // Process each item
        for (const auto& item : batch->data) {
            if (auto* str_item = item->as<StringData>()) {
                auto processed = std::make_shared<StringData>(
                    "Batch: " + str_item->value
                );
                result->data.push_back(processed);
            }
        }
        
        return result;
    }
};

// Usage
auto batch_processor = std::make_shared<BatchTextProcessor>();

// Create batch data
auto batch = std::make_shared<VectorData>();
batch->data.push_back(std::make_shared<StringData>("item1"));
batch->data.push_back(std::make_shared<StringData>("item2"));
batch->data.push_back(std::make_shared<StringData>("item3"));

// Process the entire batch
auto result = batch_processor->Execute(batch);
```

### Pattern 5: Async Batch Processing

```cpp
#include "AsyncBatchNode/AsyncBatchNode.h"

class AsyncBatchProcessor : public AsyncBatchNode {
private:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData for async batch processing");
        }
        
        // Simulate async batch processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto result = std::make_shared<VectorData>();
        result->data.reserve(batch->data.size());
        
        for (const auto& item : batch->data) {
            if (auto* str_item = item->as<StringData>()) {
                result->data.push_back(
                    std::make_shared<StringData>("AsyncBatch: " + str_item->value)
                );
            }
        }
        
        return result;
    }
};

// Usage
auto async_batch_processor = std::make_shared<AsyncBatchProcessor>();
auto batch = std::make_shared<VectorData>();
// ... populate batch ...

// Process asynchronously
auto future = async_batch_processor->RunAsync(batch);
auto result = future.get();  // Non-blocking batch processing
```

### Pattern 6: Parallel Batch Processing

```cpp
#include "AsyncParallelBatchNode/AsyncParallelBatchNode.h"

class ParallelBatchProcessor : public AsyncParallelBatchNode {
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
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        return std::make_shared<StringData>("Parallel: " + str_item->value);
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

// Usage for high-throughput scenarios
auto parallel_processor = std::make_shared<ParallelBatchProcessor>();
auto large_batch = std::make_shared<VectorData>();

// Create large batch
for (int i = 0; i < 1000; ++i) {
    large_batch->data.push_back(
        std::make_shared<StringData>("item_" + std::to_string(i))
    );
}

// Process with maximum parallelism
auto future = parallel_processor->RunAsync(large_batch);
auto result = future.get();  // All items processed in parallel
```

### Pattern 7: Advanced Flow Types

```cpp
#include "AsyncFlow/AsyncFlow.h"
#include "AsyncBatchFlow/AsyncBatchFlow.h"
#include "AsyncParallelBatchFlow/AsyncParallelBatchFlow.h"

// AsyncFlow - Non-blocking workflow execution
auto async_flow = std::make_shared<AsyncFlow>(start_node);
auto future = async_flow->RunAsync(input);

// AsyncBatchFlow - Non-blocking batch workflow
auto async_batch_flow = std::make_shared<AsyncBatchFlow>(batch_start_node);
auto batch_future = async_batch_flow->RunAsync(batch_input);

// AsyncParallelBatchFlow - Maximum performance workflow
auto parallel_flow = std::make_shared<AsyncParallelBatchFlow>(parallel_start_node);
auto parallel_future = parallel_flow->RunAsync(large_batch_input);

// All can be used with timeouts and cancellation
try {
    auto result = parallel_flow->RunAsyncWithTimeout(
        large_batch_input, 
        std::chrono::minutes(5)
    ).get();
} catch (const std::runtime_error& e) {
    std::cout << "Processing timed out" << std::endl;
}
```

## Working with Data

### Using TypedData for Type Safety

```cpp
#include "BaseNode/TypedData.h"

// Create typed data with validation
auto age = std::make_unique<TypedData<int>>(
    25,
    {Constraints<int>::range(0, 150)}  // Age must be 0-150
);

auto email = std::make_unique<TypedData<std::string>>(
    "user@example.com",
    {Constraints<std::string>::custom(
        [](const std::string& s) { return s.find('@') != std::string::npos; },
        "Must be a valid email address"
    )}
);

// Use in nodes
class UserProcessorNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // Type-safe access
        if (auto* age_data = prep_res->as<TypedData<int>>()) {
            int age = age_data->get();
            
            if (age >= 18) {
                return std::make_shared<StringData>("adult");
            } else {
                return std::make_shared<StringData>("minor");
            }
        }
        
        throw std::invalid_argument("Expected age data");
    }
};
```

### Custom Data Types

```cpp
#include "BaseNode/FrameworkData.h"

// Define your custom data structure
struct UserData : public FrameworkData {
    std::string name;
    int age;
    std::vector<std::string> tags;
    
    UserData(std::string n, int a, std::vector<std::string> t)
        : name(std::move(n)), age(a), tags(std::move(t)) {}
    
    // Required FrameworkData interface
    [[nodiscard]] std::unique_ptr<FrameworkData> clone() const override {
        return std::make_unique<UserData>(name, age, tags);
    }
    
    [[nodiscard]] const std::type_info& type() const noexcept override {
        return typeid(UserData);
    }
    
    // Optional: Serialization support
    std::string serialize() const override {
        std::ostringstream oss;
        oss << name << "," << age << ",";
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) oss << ";";
            oss << tags[i];
        }
        return oss.str();
    }
    
    bool deserialize(std::string_view data) override {
        // Implement deserialization logic
        // Return true on success, false on failure
        return false;  // Simplified for example
    }
};

// Use custom data in nodes
class UserProcessorNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* user = prep_res->as<UserData>()) {
            // Process user data
            std::cout << "Processing user: " << user->name 
                      << " (age: " << user->age << ")" << std::endl;
            
            // Create result
            auto result = std::make_shared<StringData>(
                "Processed user: " + user->name
            );
            return result;
        }
        
        throw std::invalid_argument("Expected UserData");
    }
};
```

## Error Handling

### Using Result Types for Safe Operations

```cpp
#include "BaseNode/Result.h"

using namespace framework;

// Function that can fail
Result<std::shared_ptr<FrameworkData>> safe_divide(double a, double b) {
    if (b == 0.0) {
        return FrameworkError::invalid_argument("Division by zero");
    }
    
    auto result = std::make_shared<TypedData<double>>(a / b);
    return std::static_pointer_cast<FrameworkData>(result);
}

// Node using Result types
class SafeMathNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // Chain operations with automatic error propagation
        auto result = extract_numbers(prep_res)
            .flat_map([](auto numbers) { return safe_divide(numbers.first, numbers.second); })
            .map([](auto data) {
                std::cout << "Division successful" << std::endl;
                return data;
            })
            .map_error([](const FrameworkError& e) {
                std::cerr << "Math operation failed: " << e.what() << std::endl;
                return e;
            });
        
        if (result.has_value()) {
            return result.value();
        } else {
            // Convert error to exception (or handle differently)
            throw result.error();
        }
    }
    
private:
    Result<std::pair<double, double>> extract_numbers(std::shared_ptr<FrameworkData> data) {
        // Implementation to extract two numbers from input data
        // Return Result with pair of numbers or error
        return std::make_pair(10.0, 2.0);  // Simplified
    }
};
```

### Structured Error Handling

```cpp
#include "BaseNode/FrameworkError.h"

class RobustProcessorNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        try {
            return process_data(prep_res);
        } catch (const FrameworkError& e) {
            // Framework-specific error handling
            log_error("Framework error", e);
            throw;  // Re-throw or handle as appropriate
        } catch (const std::invalid_argument& e) {
            // Convert to framework error with context
            throw FrameworkError::invalid_argument(e.what(), "RobustProcessorNode::Exec");
        } catch (const std::exception& e) {
            // Generic error handling
            throw FrameworkError::runtime_error(e.what(), "RobustProcessorNode::Exec");
        }
    }
    
private:
    std::shared_ptr<FrameworkData> process_data(std::shared_ptr<FrameworkData> data) {
        // Your processing logic here
        if (!data) {
            throw FrameworkError::invalid_argument("Input data cannot be null");
        }
        
        // Process and return result
        return data;
    }
    
    void log_error(const std::string& category, const FrameworkError& error) {
        std::cerr << "[" << category << "] " 
                  << "Error in " << error.context() << ": " 
                  << error.what() << std::endl;
    }
};
```

## Async Operations

### Basic Async Processing

```cpp
#include "AsyncNode/AsyncNode.h"
#include "BaseNode/CancellationToken.h"

class AsyncDataFetcher : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [prep_res]() {
            // Simulate network request or file I/O
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            if (auto* url = prep_res->as<StringData>()) {
                // Simulate fetched data
                auto result = std::make_shared<StringData>(
                    "Data from: " + url->value
                );
                return std::static_pointer_cast<FrameworkData>(result);
            }
            
            throw std::invalid_argument("Expected URL string");
        });
    }
};

// Usage with timeout and cancellation
int main() {
    auto fetcher = std::make_shared<AsyncDataFetcher>();
    auto url = std::make_shared<StringData>("https://api.example.com/data");
    
    try {
        // With timeout
        auto future = fetcher->RunAsyncWithTimeout(
            url, 
            std::chrono::seconds(5)
        );
        auto result = future.get();
        std::cout << "Fetch completed" << std::endl;
        
    } catch (const std::runtime_error& e) {
        std::cerr << "Fetch timed out: " << e.what() << std::endl;
    }
    
    // With cancellation
    CancellationToken token;
    auto cancel_future = fetcher->RunAsyncWithCancellation(url, token);
    
    // Cancel after 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));
    token.cancel();
    
    try {
        auto result = cancel_future.get();
    } catch (const std::runtime_error& e) {
        std::cout << "Operation cancelled" << std::endl;
    }
    
    return 0;
}
```

### Async Flow Coordination

```cpp
#include "AsyncFlow/AsyncFlow.h"

class AsyncFlow : public Flow {
public:
    // Override to handle async nodes in the flow
    std::shared_ptr<FrameworkData> orch(
        std::shared_ptr<FrameworkData> shared,
        const std::map<std::string, std::shared_ptr<FrameworkData>>& params = {}) override {
        
        auto current_node = startNode;
        auto current_data = shared;
        
        while (current_node) {
            // Check if node is async
            if (auto* async_node = dynamic_cast<AsyncNode*>(current_node.get())) {
                // Handle async execution
                auto future = async_node->RunAsync(current_data);
                current_data = future.get();  // Wait for completion
            } else {
                // Handle sync execution
                current_data = current_node->Execute(current_data);
            }
            
            // Get next node (simplified)
            current_node = GetNextNode(current_node, "default");
        }
        
        return current_data;
    }
};
```

## Best Practices

### 1. Memory Management

```cpp
// ✅ Good: Use smart pointers
auto data = std::make_shared<StringData>("value");
auto node = std::make_shared<MyNode>();

// ❌ Bad: Raw pointers
StringData* data = new StringData("value");  // Memory leak risk
```

### 2. Exception Safety

```cpp
class SafeNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // ✅ Good: RAII and exception safety
        std::unique_ptr<Resource> resource = acquire_resource();
        
        try {
            auto result = process_with_resource(prep_res, resource.get());
            return result;  // Resource automatically cleaned up
        } catch (...) {
            // Resource automatically cleaned up by unique_ptr
            throw;
        }
    }
};
```

### 3. Parameter Validation

```cpp
class ValidatingNode : public Node {
public:
    std::shared_ptr<FrameworkData> Prep(std::shared_ptr<FrameworkData> shared) override {
        // ✅ Good: Validate parameters early
        validateParams();
        
        auto required_param = getParamSafe("required_config");
        if (!required_param) {
            throw FrameworkError::invalid_argument(
                "Missing required parameter: required_config",
                "ValidatingNode::Prep"
            );
        }
        
        return shared;
    }
    
    void validateParams() const override {
        // Implement parameter validation logic
        auto timeout = getParam("timeout");
        if (timeout) {
            if (auto* timeout_data = timeout->as<TypedData<int>>()) {
                if (timeout_data->get() <= 0) {
                    throw FrameworkError::invalid_argument(
                        "Timeout must be positive",
                        "ValidatingNode::validateParams"
                    );
                }
            }
        }
    }
};
```

### 4. Resource Cleanup

```cpp
class ResourceManagedNode : public AsyncNode {
private:
    std::unique_ptr<ExpensiveResource> resource_;
    
public:
    ResourceManagedNode() : resource_(std::make_unique<ExpensiveResource>()) {}
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        // Capture resource in lambda for safe async access
        auto resource_ptr = resource_.get();
        
        return std::async(std::launch::async, [prep_res, resource_ptr]() {
            // Use resource safely
            return resource_ptr->process(prep_res);
        });
    }
    
private:
    void cleanup_resources() noexcept override {
        // Custom cleanup if needed
        resource_.reset();
    }
};
```

### 5. Performance Optimization

```cpp
class OptimizedNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // ✅ Good: Reserve container capacity
        std::vector<std::string> results;
        results.reserve(expected_size);
        
        // ✅ Good: Use move semantics
        return std::make_shared<StringData>(std::move(processed_string));
    }
    
private:
    static constexpr size_t expected_size = 1000;
};
```

### 6. Thread Safety

```cpp
class ThreadSafeNode : public AsyncNode {
private:
    mutable std::shared_mutex data_mutex_;
    std::map<std::string, std::string> cached_data_;
    
public:
    std::string get_cached_value(const std::string& key) const {
        // ✅ Good: Shared lock for read operations
        std::shared_lock lock(data_mutex_);
        auto it = cached_data_.find(key);
        return it != cached_data_.end() ? it->second : "";
    }
    
    void set_cached_value(const std::string& key, const std::string& value) {
        // ✅ Good: Unique lock for write operations
        std::unique_lock lock(data_mutex_);
        cached_data_[key] = value;
    }
};
```

This getting started guide provides a comprehensive introduction to the C++ Agentic Framework with practical examples and best practices for building robust, efficient processing pipelines.