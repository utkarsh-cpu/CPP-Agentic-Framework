# Async Timeout and Cancellation Usage Guide

This document explains how to use the timeout and cancellation features implemented in the C++ Agentic Framework.

## Overview

The framework now provides comprehensive timeout and cancellation support for async operations through several key components:

- **CancellationToken**: Thread-safe cancellation signaling
- **TimeoutPolicy**: Configurable timeout policies for different operation types
- **AsyncTimeout**: Template functions for executing operations with timeout
- **AsyncNode Integration**: Enhanced AsyncNode with timeout and cancellation support

## Core Components

### 1. CancellationToken

The `CancellationToken` provides a thread-safe way to signal cancellation to long-running operations.

```cpp
#include "BaseNode/CancellationToken.h"

// Create a cancellation token source
CancellationTokenSource token_source;

// Check if cancellation was requested
if (token_source.token().is_cancellation_requested()) {
    // Handle cancellation
}

// Throw if cancellation was requested
token_source.token().throw_if_cancellation_requested();

// Register a callback for when cancellation occurs
token_source.token().register_callback([]() {
    std::cout << "Operation was cancelled!" << std::endl;
});

// Cancel the operation
token_source.cancel();
```

### 2. TimeoutPolicy

`TimeoutPolicy` allows you to define timeout behavior for different types of operations.

```cpp
#include "BaseNode/TimeoutPolicy.h"

// Use predefined policies
auto fast_policy = TimeoutPolicy::for_operation(TimeoutPolicy::OperationType::FAST_OPERATION);
auto normal_policy = TimeoutPolicy::for_operation(TimeoutPolicy::OperationType::NORMAL_OPERATION);
auto batch_policy = TimeoutPolicy::for_operation(TimeoutPolicy::OperationType::BATCH_OPERATION);

// Create custom policy
auto custom_policy = TimeoutPolicy(
    std::chrono::seconds(30),    // timeout duration
    true,                        // enable cancellation
    std::chrono::seconds(5)      // cleanup timeout
);

// Register named policies
TimeoutPolicyRegistry::instance().register_policy("my_policy", custom_policy);
```

### 3. Execute with Timeout

The framework provides template functions for executing operations with timeout support.

```cpp
#include "BaseNode/AsyncTimeout.h"

// Simple timeout execution
auto simple_work = [](int duration_seconds) -> std::string {
    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    return "Work completed";
};

try {
    auto result = execute_with_timeout(simple_work, std::chrono::seconds(5), 2);
    std::cout << "Result: " << result << std::endl;
} catch (const TimeoutException& e) {
    std::cout << "Operation timed out: " << e.what() << std::endl;
}

// Cancellation-aware execution
auto cancellable_work = [](const CancellationToken& token, int iterations) -> std::string {
    for (int i = 0; i < iterations; ++i) {
        token.throw_if_cancellation_requested();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return "Cancellable work completed";
};

try {
    auto result = execute_with_timeout_and_cancellation(
        cancellable_work, 
        std::chrono::seconds(2), 
        50  // 50 iterations of 100ms each
    );
    std::cout << "Result: " << result << std::endl;
} catch (const TimeoutException& e) {
    std::cout << "Operation timed out: " << e.what() << std::endl;
} catch (const OperationCancelledException& e) {
    std::cout << "Operation was cancelled: " << e.what() << std::endl;
}
```

### 4. AsyncNode Integration

The `AsyncNode` class now supports timeout and cancellation through new methods.

```cpp
#include "AsyncNode/AsyncNode.h"

class MyAsyncNode : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes) override {
        return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
            // Your async work here
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return std::make_shared<StringData>("Work completed");
        });
    }

protected:
    std::future<std::shared_ptr<FrameworkData>> ExecAsyncWithCancellation(
        std::shared_ptr<FrameworkData> PrepRes, const CancellationToken& token) override {
        
        return std::async(std::launch::async, [this, PrepRes, &token]() -> std::shared_ptr<FrameworkData> {
            // Check for cancellation periodically
            for (int i = 0; i < 20; ++i) {
                token.throw_if_cancellation_requested();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            return std::make_shared<StringData>("Cancellable work completed");
        });
    }
};

// Usage
auto node = std::make_shared<MyAsyncNode>();
auto input_data = std::make_shared<StringData>("input");

// Execute with timeout
try {
    auto future = node->RunAsyncWithTimeout(input_data, std::chrono::seconds(5));
    auto result = future.get();
} catch (const TimeoutException& e) {
    std::cout << "Node execution timed out" << std::endl;
}

// Execute with policy
try {
    auto future = node->RunAsyncWithPolicy(input_data, "fast");
    auto result = future.get();
} catch (const TimeoutException& e) {
    std::cout << "Node execution timed out with fast policy" << std::endl;
}

// Execute with cancellation
CancellationTokenSource token_source;
try {
    auto future = node->RunAsyncWithCancellation(input_data, token_source.token());
    
    // Cancel after some time
    std::thread canceller([&token_source]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        token_source.cancel();
    });
    
    auto result = future.get();
    canceller.join();
} catch (const OperationCancelledException& e) {
    std::cout << "Node execution was cancelled" << std::endl;
}
```

## Exception Types

The framework defines specific exception types for timeout and cancellation scenarios:

### TimeoutException

Thrown when an operation exceeds its timeout duration.

```cpp
try {
    // Some operation that might timeout
} catch (const TimeoutException& e) {
    std::cout << "Timeout: " << e.what() << std::endl;
    std::cout << "Duration: " << e.timeout_duration().count() << "ms" << std::endl;
    std::cout << "Category: " << static_cast<int>(e.category()) << std::endl;
}
```

### OperationCancelledException

Thrown when an operation is cancelled via a cancellation token.

```cpp
try {
    // Some cancellable operation
} catch (const OperationCancelledException& e) {
    std::cout << "Cancelled: " << e.what() << std::endl;
    std::cout << "Category: " << static_cast<int>(e.category()) << std::endl;
}
```

## Best Practices

### 1. Choose Appropriate Timeouts

- **Fast operations**: 1-5 seconds
- **Normal operations**: 30 seconds
- **Batch operations**: 5-15 minutes
- **Network operations**: 10-30 seconds

### 2. Implement Cancellation Checking

For long-running operations, check for cancellation regularly:

```cpp
auto long_running_work = [](const CancellationToken& token) {
    for (int i = 0; i < 1000; ++i) {
        // Check for cancellation every iteration
        token.throw_if_cancellation_requested();
        
        // Do some work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};
```

### 3. Handle Cleanup Properly

Always ensure proper cleanup in your async operations:

```cpp
class MyAsyncNode : public AsyncNode {
protected:
    void cleanup_resources() noexcept override {
        try {
            // Close file handles
            // Release network connections
            // Clear temporary buffers
            // Reset internal state
        } catch (...) {
            // Log error but don't propagate
        }
    }
};
```

### 4. Use Appropriate Policies

Register and use named policies for consistency:

```cpp
// At application startup
TimeoutPolicyRegistry::instance().register_policy(
    "database_query", 
    TimeoutPolicy(std::chrono::seconds(30), true, std::chrono::seconds(5))
);

TimeoutPolicyRegistry::instance().register_policy(
    "file_upload", 
    TimeoutPolicy(std::chrono::minutes(5), true, std::chrono::seconds(10))
);

// In your code
auto future = node->RunAsyncWithPolicy(data, "database_query");
```

## Testing

The framework includes comprehensive tests for timeout and cancellation functionality. Key test areas include:

- Basic timeout functionality
- Cancellation token behavior
- Timeout policy configuration
- Exception handling
- Integration with AsyncNode

## Limitations

1. **Thread Safety**: The current implementation has some threading complexity that may cause issues in certain scenarios.
2. **Cleanup Guarantees**: While cleanup is attempted, there's no guarantee that all resources will be cleaned up if a thread is forcibly terminated.
3. **Platform Dependencies**: Some timeout mechanisms may behave differently across platforms.

## Future Improvements

1. **Coroutine Support**: Integration with C++20 coroutines for better async programming
2. **Hierarchical Cancellation**: Support for parent-child cancellation relationships
3. **Progress Reporting**: Integration with progress reporting mechanisms
4. **Retry Policies**: Automatic retry with exponential backoff for failed operations