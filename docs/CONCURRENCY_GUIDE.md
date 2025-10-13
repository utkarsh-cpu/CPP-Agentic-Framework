# Concurrency and Thread Safety Guide

## Table of Contents

1. [Overview](#overview)
2. [Thread Safety Guarantees](#thread-safety-guarantees)
3. [Synchronization Primitives](#synchronization-primitives)
4. [Async Programming Patterns](#async-programming-patterns)
5. [Cancellation and Timeouts](#cancellation-and-timeouts)
6. [Lock-Free Programming](#lock-free-programming)
7. [Common Pitfalls and Solutions](#common-pitfalls-and-solutions)
8. [Best Practices](#best-practices)

## Overview

The C++ Agentic Framework is designed with thread safety as a core principle. This guide explains the concurrency model, thread safety guarantees, and best practices for building high-performance concurrent applications.

### Concurrency Model

The framework uses a hybrid concurrency model:

1. **Shared-Nothing Architecture**: Each node processes data independently
2. **Immutable Data**: FrameworkData objects are immutable after construction
3. **Thread-Safe Operations**: All public APIs are thread-safe
4. **Async-First Design**: Built-in support for asynchronous operations

## Thread Safety Guarantees

### BaseNode Thread Safety

All BaseNode operations are thread-safe and can be called concurrently from multiple threads:

```cpp
#include "BaseNode/BaseNode.h"
#include <thread>
#include <vector>

class ThreadSafeExample {
public:
    void demonstrate_thread_safety() {
        auto node = std::make_shared<MyNode>();
        
        // Multiple threads can safely access parameters
        std::vector<std::thread> threads;
        
        // Thread 1: Setting parameters
        threads.emplace_back([node]() {
            for (int i = 0; i < 1000; ++i) {
                std::map<std::string, std::shared_ptr<FrameworkData>> params;
                params["key" + std::to_string(i)] = std::make_shared<StringData>("value" + std::to_string(i));
                node->setParam(std::move(params));
            }
        });
        
        // Thread 2: Reading parameters
        threads.emplace_back([node]() {
            for (int i = 0; i < 1000; ++i) {
                auto param = node->getParam("key" + std::to_string(i % 100));
                // Safe to access param even if it's null
                if (param) {
                    // Process parameter
                }
            }
        });
        
        // Thread 3: Managing successors
        threads.emplace_back([node]() {
            auto successor = std::make_shared<MyNode>();
            for (int i = 0; i < 1000; ++i) {
                node->next(successor, "action" + std::to_string(i % 10));
            }
        });
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
    }
};
```

### FrameworkData Immutability

FrameworkData objects are designed to be immutable after construction, making them inherently thread-safe for read operations:

```cpp
#include "BaseNode/FrameworkData.h"

class ImmutableDataExample {
public:
    void demonstrate_immutable_data() {
        // Create shared data
        auto shared_data = std::make_shared<StringData>("shared value");
        
        // Multiple threads can safely read the same data
        std::vector<std::thread> readers;
        
        for (int i = 0; i < 10; ++i) {
            readers.emplace_back([shared_data, i]() {
                // Safe concurrent read access
                for (int j = 0; j < 1000; ++j) {
                    std::string value = shared_data->value;  // Thread-safe read
                    
                    // Type-safe casting is also thread-safe
                    if (auto* str_data = shared_data->as<StringData>()) {
                        std::cout << "Thread " << i << " read: " << str_data->value << std::endl;
                    }
                }
            });
        }
        
        for (auto& reader : readers) {
            reader.join();
        }
    }
};
```

### AsyncNode Execution Safety

AsyncNode prevents concurrent execution of the same node instance:

```cpp
#include "AsyncNode/AsyncNode.h"

class AsyncSafetyExample : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [prep_res]() {
            // This lambda runs in a separate thread
            // The AsyncNode ensures only one execution at a time per instance
            
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

void demonstrate_async_safety() {
    auto async_node = std::make_shared<AsyncSafetyExample>();
    auto input = std::make_shared<StringData>("test");
    
    // First execution starts
    auto future1 = async_node->RunAsync(input);
    
    try {
        // Second execution will throw because first is still running
        auto future2 = async_node->RunAsync(input);
        assert(false);  // Should not reach here
    } catch (const std::runtime_error& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
        // "AsyncNode already executing - concurrent execution not allowed"
    }
    
    // Wait for first execution to complete
    auto result = future1.get();
    
    // Now second execution can proceed
    auto future3 = async_node->RunAsync(input);
    auto result2 = future3.get();
}
```

## Synchronization Primitives

### Shared Mutex for Reader-Writer Scenarios

The framework uses `std::shared_mutex` for scenarios with many readers and few writers:

```cpp
#include <shared_mutex>

class ReaderWriterNode : public BaseNode {
private:
    mutable std::shared_mutex data_mutex_;
    std::map<std::string, std::string> cached_data_;
    
public:
    // Multiple threads can read simultaneously
    std::string read_cached_data(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(data_mutex_);
        
        auto it = cached_data_.find(key);
        return it != cached_data_.end() ? it->second : "";
    }
    
    // Only one thread can write at a time
    void write_cached_data(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(data_mutex_);
        
        cached_data_[key] = value;
    }
    
    // Read-modify-write operation
    void update_cached_data(const std::string& key, 
                           std::function<std::string(const std::string&)> updater) {
        std::unique_lock<std::shared_mutex> lock(data_mutex_);
        
        auto it = cached_data_.find(key);
        std::string current_value = it != cached_data_.end() ? it->second : "";
        cached_data_[key] = updater(current_value);
    }
    
    // Bulk read operation
    std::map<std::string, std::string> read_all_cached_data() const {
        std::shared_lock<std::shared_mutex> lock(data_mutex_);
        
        return cached_data_;  // Copy while holding shared lock
    }
};
```

### Atomic Operations for Performance-Critical Code

```cpp
#include <atomic>

class AtomicCounterNode : public Node {
private:
    std::atomic<uint64_t> processed_count_{0};
    std::atomic<uint64_t> error_count_{0};
    std::atomic<uint64_t> total_processing_time_ns_{0};
    
public:
    std::shared_ptr<FrameworkData> Execute(std::shared_ptr<FrameworkData> shared) override {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            auto result = Node::Execute(shared);
            
            // Atomic increment for success
            processed_count_.fetch_add(1, std::memory_order_relaxed);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time - start_time
            ).count();
            
            // Atomic add for timing
            total_processing_time_ns_.fetch_add(duration, std::memory_order_relaxed);
            
            return result;
            
        } catch (...) {
            // Atomic increment for errors
            error_count_.fetch_add(1, std::memory_order_relaxed);
            throw;
        }
    }
    
    // Thread-safe statistics access
    struct Statistics {
        uint64_t processed_count;
        uint64_t error_count;
        double average_processing_time_ns;
        double success_rate;
    };
    
    Statistics get_statistics() const noexcept {
        Statistics stats;
        stats.processed_count = processed_count_.load(std::memory_order_relaxed);
        stats.error_count = error_count_.load(std::memory_order_relaxed);
        
        uint64_t total_operations = stats.processed_count + stats.error_count;
        if (total_operations > 0) {
            stats.success_rate = static_cast<double>(stats.processed_count) / total_operations;
            stats.average_processing_time_ns = static_cast<double>(
                total_processing_time_ns_.load(std::memory_order_relaxed)
            ) / stats.processed_count;
        } else {
            stats.success_rate = 0.0;
            stats.average_processing_time_ns = 0.0;
        }
        
        return stats;
    }
};
```

### Condition Variables for Coordination

```cpp
#include <condition_variable>
#include <queue>

template<typename T>
class ThreadSafeQueue {
private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable condition_;
    bool shutdown_ = false;
    
public:
    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (shutdown_) {
            throw std::runtime_error("Queue is shut down");
        }
        
        queue_.push(std::move(item));
        condition_.notify_one();
    }
    
    bool pop(T& item, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        bool success = condition_.wait_for(lock, timeout, [this] {
            return !queue_.empty() || shutdown_;
        });
        
        if (!success || (queue_.empty() && shutdown_)) {
            return false;
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        condition_.notify_all();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
};

class ProducerConsumerNode : public AsyncNode {
private:
    ThreadSafeQueue<std::shared_ptr<FrameworkData>> work_queue_;
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{true};
    
public:
    ProducerConsumerNode(size_t num_workers = std::thread::hardware_concurrency()) {
        // Start worker threads
        for (size_t i = 0; i < num_workers; ++i) {
            worker_threads_.emplace_back(&ProducerConsumerNode::worker_loop, this, i);
        }
    }
    
    ~ProducerConsumerNode() {
        // Shutdown workers
        running_ = false;
        work_queue_.shutdown();
        
        for (auto& worker : worker_threads_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        auto promise = std::make_shared<std::promise<std::shared_ptr<FrameworkData>>>();
        auto future = promise->get_future();
        
        // Create work item
        auto work_item = std::make_shared<WorkItem>(prep_res, promise);
        
        // Add to queue
        work_queue_.push(work_item);
        
        return future;
    }
    
private:
    struct WorkItem : public FrameworkData {
        std::shared_ptr<FrameworkData> data;
        std::shared_ptr<std::promise<std::shared_ptr<FrameworkData>>> promise;
        
        WorkItem(std::shared_ptr<FrameworkData> d, 
                std::shared_ptr<std::promise<std::shared_ptr<FrameworkData>>> p)
            : data(std::move(d)), promise(std::move(p)) {}
        
        [[nodiscard]] std::unique_ptr<FrameworkData> clone() const override {
            return std::make_unique<WorkItem>(data->clone(), promise);
        }
        
        [[nodiscard]] const std::type_info& type() const noexcept override {
            return typeid(WorkItem);
        }
    };
    
    void worker_loop(size_t worker_id) {
        while (running_) {
            std::shared_ptr<FrameworkData> work_data;
            
            if (work_queue_.pop(work_data, std::chrono::milliseconds(100))) {
                if (auto* work_item = work_data->as<WorkItem>()) {
                    try {
                        // Process the work item
                        auto result = process_work_item(work_item->data);
                        work_item->promise->set_value(result);
                    } catch (...) {
                        work_item->promise->set_exception(std::current_exception());
                    }
                }
            }
        }
    }
    
    std::shared_ptr<FrameworkData> process_work_item(std::shared_ptr<FrameworkData> data) {
        // Actual processing logic
        if (auto* input = data->as<StringData>()) {
            return std::make_shared<StringData>("Worker processed: " + input->value);
        }
        return data;
    }
};
```

## Async Programming Patterns

### Future-Based Composition

```cpp
#include <future>

class AsyncCompositionExample {
public:
    // Chain async operations
    std::future<std::shared_ptr<FrameworkData>> chain_async_operations(
        std::shared_ptr<FrameworkData> input) {
        
        auto node1 = std::make_shared<AsyncProcessingNode>();
        auto node2 = std::make_shared<AsyncTransformNode>();
        auto node3 = std::make_shared<AsyncValidationNode>();
        
        // Chain operations using then-like pattern
        return node1->RunAsync(input)
            .then([node2](auto future) {
                auto result1 = future.get();
                return node2->RunAsync(result1);
            })
            .then([node3](auto future) {
                auto result2 = future.get();
                return node3->RunAsync(result2);
            });
    }
    
    // Parallel execution with synchronization
    std::future<std::shared_ptr<FrameworkData>> parallel_processing(
        std::shared_ptr<FrameworkData> input) {
        
        auto node1 = std::make_shared<AsyncProcessingNode>();
        auto node2 = std::make_shared<AsyncProcessingNode>();
        auto node3 = std::make_shared<AsyncProcessingNode>();
        
        return std::async(std::launch::async, [=]() {
            // Start all operations in parallel
            auto future1 = node1->RunAsync(input);
            auto future2 = node2->RunAsync(input);
            auto future3 = node3->RunAsync(input);
            
            // Wait for all to complete
            auto result1 = future1.get();
            auto result2 = future2.get();
            auto result3 = future3.get();
            
            // Combine results
            return combine_results({result1, result2, result3});
        });
    }
    
private:
    std::shared_ptr<FrameworkData> combine_results(
        const std::vector<std::shared_ptr<FrameworkData>>& results) {
        
        auto combined = std::make_shared<VectorData>();
        combined->data = results;
        return combined;
    }
};

// Extension to std::future for chaining (C++20 style)
template<typename T>
class ChainableFuture {
private:
    std::future<T> future_;
    
public:
    explicit ChainableFuture(std::future<T> future) : future_(std::move(future)) {}
    
    template<typename F>
    auto then(F&& func) -> ChainableFuture<std::invoke_result_t<F, std::future<T>&>> {
        using ReturnType = std::invoke_result_t<F, std::future<T>&>;
        
        auto promise = std::make_shared<std::promise<ReturnType>>();
        auto result_future = promise->get_future();
        
        std::thread([promise, func = std::forward<F>(func), future = std::move(future_)]() mutable {
            try {
                if constexpr (std::is_void_v<ReturnType>) {
                    func(future);
                    promise->set_value();
                } else {
                    auto result = func(future);
                    promise->set_value(std::move(result));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
        
        return ChainableFuture<ReturnType>(std::move(result_future));
    }
    
    T get() {
        return future_.get();
    }
    
    template<typename Rep, typename Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) {
        return future_.wait_for(timeout_duration);
    }
};

template<typename T>
ChainableFuture<T> make_chainable(std::future<T> future) {
    return ChainableFuture<T>(std::move(future));
}
```

### Coroutine Support (C++20)

```cpp
#ifdef __cpp_impl_coroutine
#include <coroutine>

template<typename T>
class Task {
public:
    struct promise_type {
        T value_;
        std::exception_ptr exception_;
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        
        void return_value(T value) {
            value_ = std::move(value);
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    explicit Task(handle_type handle) : handle_(handle) {}
    
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    T get() {
        if (!handle_) {
            throw std::runtime_error("Task is empty");
        }
        
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        
        return std::move(handle_.promise().value_);
    }
    
    bool is_ready() const {
        return handle_ && handle_.done();
    }
    
private:
    handle_type handle_;
};

class CoroutineAsyncNode : public AsyncNode {
public:
    Task<std::shared_ptr<FrameworkData>> process_async_coroutine(
        std::shared_ptr<FrameworkData> input) {
        
        // Simulate async work
        co_await std::suspend_for(std::chrono::milliseconds(100));
        
        if (auto* str_data = input->as<StringData>()) {
            auto result = std::make_shared<StringData>(
                "Coroutine processed: " + str_data->value
            );
            co_return std::static_pointer_cast<FrameworkData>(result);
        }
        
        throw std::invalid_argument("Expected StringData");
    }
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [this, prep_res]() {
            auto task = process_async_coroutine(prep_res);
            
            // Wait for coroutine to complete
            while (!task.is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            return task.get();
        });
    }
};

// Awaitable for framework data processing
struct FrameworkDataAwaitable {
    std::shared_ptr<AsyncNode> node;
    std::shared_ptr<FrameworkData> input;
    
    bool await_ready() const { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) {
        auto future = node->RunAsync(input);
        
        std::thread([future = std::move(future), handle]() mutable {
            try {
                auto result = future.get();
                handle.resume();
            } catch (...) {
                handle.resume();
            }
        }).detach();
    }
    
    std::shared_ptr<FrameworkData> await_resume() {
        auto future = node->RunAsync(input);
        return future.get();
    }
};

Task<std::shared_ptr<FrameworkData>> process_pipeline_coroutine(
    std::shared_ptr<FrameworkData> input) {
    
    auto node1 = std::make_shared<AsyncProcessingNode>();
    auto node2 = std::make_shared<AsyncTransformNode>();
    auto node3 = std::make_shared<AsyncValidationNode>();
    
    // Process through pipeline using coroutines
    auto result1 = co_await FrameworkDataAwaitable{node1, input};
    auto result2 = co_await FrameworkDataAwaitable{node2, result1};
    auto result3 = co_await FrameworkDataAwaitable{node3, result2};
    
    co_return result3;
}
#endif
```

## Cancellation and Timeouts

### CancellationToken Usage

```cpp
#include "BaseNode/CancellationToken.h"

class CancellableAsyncNode : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> RunAsyncWithCancellation(
        std::shared_ptr<FrameworkData> shared,
        const CancellationToken& token) override {
        
        return std::async(std::launch::async, [this, shared, &token]() {
            return process_with_cancellation(shared, token);
        });
    }
    
private:
    std::shared_ptr<FrameworkData> process_with_cancellation(
        std::shared_ptr<FrameworkData> data,
        const CancellationToken& token) {
        
        // Long-running operation with cancellation checks
        for (int i = 0; i < 1000; ++i) {
            // Check for cancellation periodically
            token.throw_if_cancelled();
            
            // Simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Alternative: graceful cancellation check
            if (token.is_cancelled()) {
                std::cout << "Operation cancelled gracefully at step " << i << std::endl;
                return create_partial_result(data, i);
            }
        }
        
        return create_complete_result(data);
    }
    
    std::shared_ptr<FrameworkData> create_partial_result(
        std::shared_ptr<FrameworkData> input, int steps_completed) {
        
        if (auto* str_data = input->as<StringData>()) {
            return std::make_shared<StringData>(
                "Partial result (" + std::to_string(steps_completed) + " steps): " + str_data->value
            );
        }
        return input;
    }
    
    std::shared_ptr<FrameworkData> create_complete_result(std::shared_ptr<FrameworkData> input) {
        if (auto* str_data = input->as<StringData>()) {
            return std::make_shared<StringData>("Complete result: " + str_data->value);
        }
        return input;
    }
};

// Usage example
void demonstrate_cancellation() {
    auto node = std::make_shared<CancellableAsyncNode>();
    auto input = std::make_shared<StringData>("test data");
    
    CancellationToken token;
    
    // Start long-running operation
    auto future = node->RunAsyncWithCancellation(input, token);
    
    // Cancel after 2 seconds
    std::thread canceller([&token]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        token.cancel();
        std::cout << "Cancellation requested" << std::endl;
    });
    
    try {
        auto result = future.get();
        std::cout << "Operation completed" << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "Operation cancelled: " << e.what() << std::endl;
    }
    
    canceller.join();
}
```

### Timeout Policies

```cpp
#include "BaseNode/TimeoutPolicy.h"

class TimeoutAwareNode : public AsyncNode {
private:
    TimeoutPolicy default_policy_;
    
public:
    TimeoutAwareNode() {
        // Configure default timeout policy
        default_policy_.set_operation_timeout(std::chrono::seconds(30));
        default_policy_.set_retry_timeout(std::chrono::seconds(5));
        default_policy_.set_max_retries(3);
    }
    
    std::future<std::shared_ptr<FrameworkData>> RunAsyncWithPolicy(
        std::shared_ptr<FrameworkData> shared,
        const TimeoutPolicy& policy) override {
        
        return std::async(std::launch::async, [this, shared, policy]() {
            return execute_with_timeout_policy(shared, policy);
        });
    }
    
private:
    std::shared_ptr<FrameworkData> execute_with_timeout_policy(
        std::shared_ptr<FrameworkData> data,
        const TimeoutPolicy& policy) {
        
        int retry_count = 0;
        
        while (retry_count <= policy.get_max_retries()) {
            try {
                // Execute with timeout
                auto future = std::async(std::launch::async, [this, data]() {
                    return perform_operation(data);
                });
                
                auto status = future.wait_for(policy.get_operation_timeout());
                
                if (status == std::future_status::ready) {
                    return future.get();  // Success
                } else {
                    // Timeout occurred
                    if (retry_count < policy.get_max_retries()) {
                        std::cout << "Operation timed out, retrying... (attempt " 
                                  << (retry_count + 1) << ")" << std::endl;
                        
                        // Wait before retry
                        std::this_thread::sleep_for(policy.get_retry_timeout());
                        retry_count++;
                        continue;
                    } else {
                        throw std::runtime_error("Operation timed out after " + 
                                               std::to_string(policy.get_max_retries()) + " retries");
                    }
                }
                
            } catch (const std::exception& e) {
                if (retry_count < policy.get_max_retries()) {
                    std::cout << "Operation failed, retrying... Error: " << e.what() << std::endl;
                    std::this_thread::sleep_for(policy.get_retry_timeout());
                    retry_count++;
                    continue;
                } else {
                    throw;  // Re-throw after max retries
                }
            }
        }
        
        throw std::runtime_error("Max retries exceeded");
    }
    
    std::shared_ptr<FrameworkData> perform_operation(std::shared_ptr<FrameworkData> data) {
        // Simulate potentially slow operation
        std::this_thread::sleep_for(std::chrono::milliseconds(
            std::rand() % 5000  // 0-5 seconds
        ));
        
        if (auto* str_data = data->as<StringData>()) {
            return std::make_shared<StringData>("Processed with timeout: " + str_data->value);
        }
        return data;
    }
};
```

## Lock-Free Programming

### Lock-Free Data Structures

```cpp
#include <atomic>

template<typename T>
class LockFreeStack {
private:
    struct Node {
        T data;
        std::atomic<Node*> next;
        
        Node(T d) : data(std::move(d)), next(nullptr) {}
    };
    
    std::atomic<Node*> head_{nullptr};
    
public:
    ~LockFreeStack() {
        while (Node* old_head = head_.load()) {
            head_ = old_head->next;
            delete old_head;
        }
    }
    
    void push(T item) {
        Node* new_node = new Node(std::move(item));
        Node* old_head = head_.load();
        
        do {
            new_node->next = old_head;
        } while (!head_.compare_exchange_weak(old_head, new_node));
    }
    
    bool pop(T& result) {
        Node* old_head = head_.load();
        
        while (old_head && !head_.compare_exchange_weak(old_head, old_head->next.load())) {
            // Retry if CAS failed
        }
        
        if (old_head) {
            result = std::move(old_head->data);
            delete old_head;
            return true;
        }
        
        return false;
    }
    
    bool empty() const {
        return head_.load() == nullptr;
    }
};

class LockFreeNode : public AsyncNode {
private:
    LockFreeStack<std::shared_ptr<FrameworkData>> pending_work_;
    std::atomic<bool> processing_{false};
    
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        // Add work to lock-free queue
        pending_work_.push(prep_res);
        
        return std::async(std::launch::async, [this]() {
            return process_pending_work();
        });
    }
    
private:
    std::shared_ptr<FrameworkData> process_pending_work() {
        // Try to acquire processing lock atomically
        bool expected = false;
        if (!processing_.compare_exchange_strong(expected, true)) {
            // Another thread is already processing
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return process_pending_work();  // Retry
        }
        
        std::shared_ptr<FrameworkData> work_item;
        std::shared_ptr<FrameworkData> result;
        
        if (pending_work_.pop(work_item)) {
            try {
                result = process_single_item(work_item);
            } catch (...) {
                processing_ = false;
                throw;
            }
        }
        
        processing_ = false;
        return result ? result : std::make_shared<StringData>("No work available");
    }
    
    std::shared_ptr<FrameworkData> process_single_item(std::shared_ptr<FrameworkData> item) {
        if (auto* str_data = item->as<StringData>()) {
            return std::make_shared<StringData>("Lock-free processed: " + str_data->value);
        }
        return item;
    }
};
```

### Memory Ordering Considerations

```cpp
class MemoryOrderingExample {
private:
    std::atomic<int> counter_{0};
    std::atomic<bool> flag_{false};
    std::atomic<std::shared_ptr<FrameworkData>> shared_data_{nullptr};
    
public:
    // Relaxed ordering for simple counters
    void increment_counter() {
        counter_.fetch_add(1, std::memory_order_relaxed);
    }
    
    int get_counter() const {
        return counter_.load(std::memory_order_relaxed);
    }
    
    // Acquire-release for synchronization
    void publish_data(std::shared_ptr<FrameworkData> data) {
        shared_data_.store(data, std::memory_order_relaxed);
        flag_.store(true, std::memory_order_release);  // Release
    }
    
    std::shared_ptr<FrameworkData> consume_data() {
        if (flag_.load(std::memory_order_acquire)) {  // Acquire
            return shared_data_.load(std::memory_order_relaxed);
        }
        return nullptr;
    }
    
    // Sequential consistency for critical operations
    bool try_lock_critical_section() {
        bool expected = false;
        return flag_.compare_exchange_strong(expected, true, std::memory_order_seq_cst);
    }
    
    void unlock_critical_section() {
        flag_.store(false, std::memory_order_seq_cst);
    }
};
```

## Common Pitfalls and Solutions

### Pitfall 1: Race Conditions in Lazy Initialization

```cpp
// ❌ Bad: Race condition
class BadLazyInit {
private:
    mutable std::shared_ptr<ExpensiveResource> resource_;
    
public:
    std::shared_ptr<ExpensiveResource> get_resource() const {
        if (!resource_) {  // Race condition here!
            resource_ = std::make_shared<ExpensiveResource>();
        }
        return resource_;
    }
};

// ✅ Good: Thread-safe lazy initialization
class GoodLazyInit {
private:
    mutable std::shared_ptr<ExpensiveResource> resource_;
    mutable std::once_flag init_flag_;
    
public:
    std::shared_ptr<ExpensiveResource> get_resource() const {
        std::call_once(init_flag_, [this]() {
            resource_ = std::make_shared<ExpensiveResource>();
        });
        return resource_;
    }
};
```

### Pitfall 2: Deadlock in Multiple Lock Acquisition

```cpp
// ❌ Bad: Potential deadlock
class BadMultipleLocks {
private:
    std::mutex mutex1_;
    std::mutex mutex2_;
    
public:
    void method1() {
        std::lock_guard<std::mutex> lock1(mutex1_);
        std::lock_guard<std::mutex> lock2(mutex2_);  // Potential deadlock
        // Do work
    }
    
    void method2() {
        std::lock_guard<std::mutex> lock2(mutex2_);
        std::lock_guard<std::mutex> lock1(mutex1_);  // Potential deadlock
        // Do work
    }
};

// ✅ Good: Ordered lock acquisition
class GoodMultipleLocks {
private:
    std::mutex mutex1_;
    std::mutex mutex2_;
    
public:
    void method1() {
        std::scoped_lock lock(mutex1_, mutex2_);  // Ordered acquisition
        // Do work
    }
    
    void method2() {
        std::scoped_lock lock(mutex1_, mutex2_);  // Same order
        // Do work
    }
};
```

### Pitfall 3: Exception Safety in Concurrent Code

```cpp
// ❌ Bad: Not exception safe
class BadExceptionSafety {
private:
    std::mutex mutex_;
    std::vector<std::shared_ptr<FrameworkData>> data_;
    
public:
    void add_data(std::shared_ptr<FrameworkData> item) {
        mutex_.lock();
        data_.push_back(item);  // Exception here leaves mutex locked!
        mutex_.unlock();
    }
};

// ✅ Good: Exception safe with RAII
class GoodExceptionSafety {
private:
    std::mutex mutex_;
    std::vector<std::shared_ptr<FrameworkData>> data_;
    
public:
    void add_data(std::shared_ptr<FrameworkData> item) {
        std::lock_guard<std::mutex> lock(mutex_);  // RAII ensures unlock
        data_.push_back(item);
    }
    
    // Even better: Strong exception safety
    void add_data_strong(std::shared_ptr<FrameworkData> item) {
        std::vector<std::shared_ptr<FrameworkData>> temp_data;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            temp_data = data_;  // Copy current state
        }
        
        temp_data.push_back(item);  // Modify copy (can throw)
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            data_ = std::move(temp_data);  // Commit (no-throw)
        }
    }
};
```

## Best Practices

### 1. Prefer Immutable Data

```cpp
// ✅ Good: Immutable data is inherently thread-safe
class ImmutableProcessingNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // Create new data instead of modifying existing
        if (auto* input = prep_res->as<StringData>()) {
            return std::make_shared<StringData>("Processed: " + input->value);
        }
        return prep_res;
    }
};
```

### 2. Use RAII for Resource Management

```cpp
// ✅ Good: RAII ensures proper cleanup
class RAIINode : public AsyncNode {
private:
    class ResourceGuard {
        std::unique_ptr<Resource> resource_;
    public:
        explicit ResourceGuard(std::unique_ptr<Resource> r) : resource_(std::move(r)) {}
        ~ResourceGuard() { /* cleanup happens automatically */ }
        Resource* get() const { return resource_.get(); }
    };
    
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [prep_res]() {
            ResourceGuard guard(std::make_unique<Resource>());
            
            // Use resource safely - cleanup is guaranteed
            return process_with_resource(prep_res, guard.get());
        });
    }
};
```

### 3. Minimize Lock Scope

```cpp
// ✅ Good: Minimal lock scope
class MinimalLockScope : public Node {
private:
    std::mutex data_mutex_;
    std::map<std::string, std::string> data_;
    
public:
    std::string get_and_process(const std::string& key) {
        std::string value;
        
        // Minimal lock scope for data access
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            auto it = data_.find(key);
            value = it != data_.end() ? it->second : "";
        }
        
        // Process outside of lock
        return expensive_processing(value);
    }
    
private:
    std::string expensive_processing(const std::string& input) {
        // Long-running operation without holding locks
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return "Processed: " + input;
    }
};
```

### 4. Use Atomic Operations for Simple Cases

```cpp
// ✅ Good: Atomic operations for simple counters
class AtomicCounters {
private:
    std::atomic<uint64_t> success_count_{0};
    std::atomic<uint64_t> error_count_{0};
    
public:
    void record_success() {
        success_count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    void record_error() {
        error_count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    std::pair<uint64_t, uint64_t> get_counts() const {
        return {
            success_count_.load(std::memory_order_relaxed),
            error_count_.load(std::memory_order_relaxed)
        };
    }
};
```

### 5. Design for Composability

```cpp
// ✅ Good: Thread-safe composable operations
class ComposableAsyncNode : public AsyncNode {
public:
    // Composable async operations
    std::future<std::shared_ptr<FrameworkData>> compose_with(
        std::shared_ptr<AsyncNode> other,
        std::shared_ptr<FrameworkData> input) {
        
        return RunAsync(input)
            .then([other](auto future) {
                auto intermediate = future.get();
                return other->RunAsync(intermediate);
            });
    }
    
    // Parallel composition
    std::future<std::vector<std::shared_ptr<FrameworkData>>> parallel_compose(
        const std::vector<std::shared_ptr<AsyncNode>>& nodes,
        std::shared_ptr<FrameworkData> input) {
        
        return std::async(std::launch::async, [nodes, input]() {
            std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
            futures.reserve(nodes.size());
            
            // Start all operations
            for (auto& node : nodes) {
                futures.emplace_back(node->RunAsync(input));
            }
            
            // Collect results
            std::vector<std::shared_ptr<FrameworkData>> results;
            results.reserve(futures.size());
            
            for (auto& future : futures) {
                results.emplace_back(future.get());
            }
            
            return results;
        });
    }
};
```

This concurrency guide provides comprehensive coverage of thread safety, synchronization patterns, and best practices for building robust concurrent applications with the C++ Agentic Framework.