# Performance Tuning Guide

## Table of Contents

1. [Overview](#overview)
2. [Memory Optimization](#memory-optimization)
3. [Concurrency Optimization](#concurrency-optimization)
4. [Batch Processing Optimization](#batch-processing-optimization)
5. [I/O and Network Optimization](#io-and-network-optimization)
6. [Profiling and Monitoring](#profiling-and-monitoring)
7. [Platform-Specific Optimizations](#platform-specific-optimizations)
8. [Benchmarking](#benchmarking)

## Overview

This guide provides comprehensive strategies for optimizing performance in high-throughput scenarios using the C++ Agentic Framework. The framework is designed with performance in mind, but proper configuration and usage patterns are essential for achieving optimal results.

### Performance Principles

1. **Minimize Memory Allocations**: Reuse objects and pre-allocate containers
2. **Optimize Data Locality**: Keep related data close in memory
3. **Leverage Parallelism**: Use async operations and batch processing
4. **Reduce Copying**: Use move semantics and references
5. **Profile First**: Measure before optimizing

## Memory Optimization

### Smart Pointer Usage

```cpp
#include "BaseNode/BaseNode.h"

class OptimizedNode : public Node {
private:
    // ✅ Good: Use object pooling for frequently allocated objects
    static thread_local std::vector<std::unique_ptr<StringData>> string_pool_;
    
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // Reuse pooled objects
        auto result = get_pooled_string();
        
        if (auto* input = prep_res->as<StringData>()) {
            result->value = "Processed: " + input->value;
        }
        
        return result;
    }
    
private:
    std::shared_ptr<StringData> get_pooled_string() {
        if (!string_pool_.empty()) {
            auto result = std::move(string_pool_.back());
            string_pool_.pop_back();
            return std::shared_ptr<StringData>(result.release());
        }
        return std::make_shared<StringData>();
    }
    
    static void return_to_pool(std::unique_ptr<StringData> obj) {
        obj->value.clear();  // Reset state
        string_pool_.push_back(std::move(obj));
    }
};

thread_local std::vector<std::unique_ptr<StringData>> OptimizedNode::string_pool_;
```

### Container Optimization

```cpp
class ContainerOptimizedNode : public BatchNode {
public:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData");
        }
        
        // ✅ Good: Reserve capacity to avoid reallocations
        auto result = std::make_shared<VectorData>();
        result->data.reserve(batch->data.size());
        
        // ✅ Good: Use range-based loops for better optimization
        for (const auto& item : batch->data) {
            if (auto* str_item = item->as<StringData>()) {
                // ✅ Good: Use emplace_back to construct in-place
                result->data.emplace_back(
                    std::make_shared<StringData>("Processed: " + str_item->value)
                );
            }
        }
        
        return result;
    }
};
```

### Memory Pool Implementation

```cpp
#include <memory_resource>

class MemoryPoolNode : public Node {
private:
    // Use polymorphic memory resource for custom allocation
    std::pmr::unsynchronized_pool_resource pool_;
    std::pmr::polymorphic_allocator<char> allocator_;
    
public:
    MemoryPoolNode() : allocator_(&pool_) {}
    
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // Use custom allocator for string operations
        std::pmr::string result(allocator_);
        
        if (auto* input = prep_res->as<StringData>()) {
            result = "Processed: " + input->value;
        }
        
        // Convert back to standard string for framework compatibility
        return std::make_shared<StringData>(std::string(result));
    }
};
```

### Memory Tracking and Leak Detection

```cpp
#include "BaseNode/MemoryTracker.h"

class MonitoredNode : public Node {
private:
    mutable MemoryTracker memory_tracker_;
    
public:
    std::shared_ptr<FrameworkData> Execute(std::shared_ptr<FrameworkData> shared) override {
        // Start memory tracking
        memory_tracker_.start_tracking();
        
        auto result = Node::Execute(shared);
        
        // Check for memory leaks
        auto usage = memory_tracker_.get_current_usage();
        if (usage.bytes_allocated > expected_memory_usage_) {
            std::cerr << "Warning: High memory usage detected: " 
                      << usage.bytes_allocated << " bytes" << std::endl;
        }
        
        return result;
    }
    
private:
    static constexpr size_t expected_memory_usage_ = 1024 * 1024;  // 1MB
};
```

## Concurrency Optimization

### Thread Pool Configuration

```cpp
#include "BaseNode/OptimizedBatchProcessor.h"

class ConcurrencyOptimizedFlow : public Flow {
private:
    OptimizedBatchProcessor batch_processor_;
    
public:
    ConcurrencyOptimizedFlow() {
        // Configure thread pool based on hardware
        size_t thread_count = std::thread::hardware_concurrency();
        
        // For CPU-intensive tasks, use all cores
        if (is_cpu_intensive_) {
            batch_processor_.set_thread_count(thread_count);
        } else {
            // For I/O-intensive tasks, use more threads than cores
            batch_processor_.set_thread_count(thread_count * 2);
        }
        
        // Enable SIMD optimizations if available
        batch_processor_.enable_simd(true);
    }
    
    std::shared_ptr<FrameworkData> process_large_dataset(
        const std::vector<std::shared_ptr<FrameworkData>>& dataset) {
        
        // Process in parallel batches
        auto results = batch_processor_.process_batch(
            dataset,
            [this](const std::shared_ptr<FrameworkData>& item) {
                return process_single_item(item);
            }
        );
        
        // Combine results
        auto combined = std::make_shared<VectorData>();
        combined->data.reserve(results.size());
        
        for (auto& result : results) {
            combined->data.emplace_back(std::move(result));
        }
        
        return combined;
    }
    
private:
    bool is_cpu_intensive_ = true;
    
    std::shared_ptr<FrameworkData> process_single_item(
        const std::shared_ptr<FrameworkData>& item) {
        // Single item processing logic
        return item;
    }
};
```

### Lock-Free Data Structures

```cpp
#include <atomic>
#include <memory>

class LockFreeNode : public AsyncNode {
private:
    // Lock-free counter for statistics
    std::atomic<uint64_t> processed_count_{0};
    std::atomic<uint64_t> error_count_{0};
    
    // Lock-free stack for object pooling
    struct PoolNode {
        std::shared_ptr<StringData> data;
        std::atomic<PoolNode*> next;
        
        PoolNode(std::shared_ptr<StringData> d) : data(std::move(d)), next(nullptr) {}
    };
    
    std::atomic<PoolNode*> pool_head_{nullptr};
    
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [this, prep_res]() {
            try {
                auto result = process_data(prep_res);
                processed_count_.fetch_add(1, std::memory_order_relaxed);
                return result;
            } catch (...) {
                error_count_.fetch_add(1, std::memory_order_relaxed);
                throw;
            }
        });
    }
    
    // Lock-free statistics access
    uint64_t get_processed_count() const noexcept {
        return processed_count_.load(std::memory_order_relaxed);
    }
    
    uint64_t get_error_count() const noexcept {
        return error_count_.load(std::memory_order_relaxed);
    }
    
private:
    std::shared_ptr<FrameworkData> process_data(std::shared_ptr<FrameworkData> data) {
        // Get object from lock-free pool
        auto pooled_obj = pop_from_pool();
        if (!pooled_obj) {
            pooled_obj = std::make_shared<StringData>();
        }
        
        // Process data
        if (auto* input = data->as<StringData>()) {
            pooled_obj->value = "Processed: " + input->value;
        }
        
        return pooled_obj;
    }
    
    std::shared_ptr<StringData> pop_from_pool() {
        PoolNode* head = pool_head_.load(std::memory_order_acquire);
        
        while (head != nullptr) {
            PoolNode* next = head->next.load(std::memory_order_relaxed);
            
            if (pool_head_.compare_exchange_weak(head, next, 
                                               std::memory_order_release,
                                               std::memory_order_acquire)) {
                auto result = head->data;
                delete head;
                return result;
            }
        }
        
        return nullptr;
    }
    
    void push_to_pool(std::shared_ptr<StringData> obj) {
        auto* new_node = new PoolNode(std::move(obj));
        PoolNode* head = pool_head_.load(std::memory_order_relaxed);
        
        do {
            new_node->next.store(head, std::memory_order_relaxed);
        } while (!pool_head_.compare_exchange_weak(head, new_node,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed));
    }
};
```

### Work Stealing Implementation

```cpp
#include <deque>
#include <mutex>
#include <condition_variable>

class WorkStealingProcessor {
private:
    struct WorkerThread {
        std::deque<std::function<void()>> local_queue;
        std::mutex queue_mutex;
        std::condition_variable condition;
        std::atomic<bool> running{true};
        std::thread thread;
        
        WorkerThread(WorkStealingProcessor* processor, size_t id) 
            : thread(&WorkStealingProcessor::worker_loop, processor, id) {}
    };
    
    std::vector<std::unique_ptr<WorkerThread>> workers_;
    std::atomic<size_t> next_worker_{0};
    
public:
    explicit WorkStealingProcessor(size_t thread_count = std::thread::hardware_concurrency()) {
        workers_.reserve(thread_count);
        
        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back(std::make_unique<WorkerThread>(this, i));
        }
    }
    
    ~WorkStealingProcessor() {
        // Signal all workers to stop
        for (auto& worker : workers_) {
            worker->running = false;
            worker->condition.notify_one();
        }
        
        // Wait for all workers to finish
        for (auto& worker : workers_) {
            if (worker->thread.joinable()) {
                worker->thread.join();
            }
        }
    }
    
    template<typename F>
    void submit(F&& task) {
        // Round-robin task distribution
        size_t worker_id = next_worker_.fetch_add(1) % workers_.size();
        auto& worker = workers_[worker_id];
        
        {
            std::lock_guard<std::mutex> lock(worker->queue_mutex);
            worker->local_queue.emplace_back(std::forward<F>(task));
        }
        
        worker->condition.notify_one();
    }
    
private:
    void worker_loop(size_t worker_id) {
        auto& worker = workers_[worker_id];
        
        while (worker->running) {
            std::function<void()> task;
            
            // Try to get task from local queue
            {
                std::unique_lock<std::mutex> lock(worker->queue_mutex);
                
                if (worker->local_queue.empty()) {
                    // Try to steal work from other workers
                    task = steal_work(worker_id);
                    
                    if (!task) {
                        // Wait for new work
                        worker->condition.wait(lock, [&] {
                            return !worker->local_queue.empty() || !worker->running;
                        });
                        
                        if (!worker->running) break;
                    }
                }
                
                if (!task && !worker->local_queue.empty()) {
                    task = std::move(worker->local_queue.front());
                    worker->local_queue.pop_front();
                }
            }
            
            // Execute task
            if (task) {
                try {
                    task();
                } catch (const std::exception& e) {
                    std::cerr << "Worker " << worker_id << " task failed: " 
                              << e.what() << std::endl;
                }
            }
        }
    }
    
    std::function<void()> steal_work(size_t thief_id) {
        // Try to steal from other workers
        for (size_t i = 1; i < workers_.size(); ++i) {
            size_t victim_id = (thief_id + i) % workers_.size();
            auto& victim = workers_[victim_id];
            
            std::lock_guard<std::mutex> lock(victim->queue_mutex);
            if (!victim->local_queue.empty()) {
                // Steal from the back (LIFO for better cache locality)
                auto task = std::move(victim->local_queue.back());
                victim->local_queue.pop_back();
                return task;
            }
        }
        
        return nullptr;
    }
};
```

## Batch Processing Optimization

### Optimized Batch Container

```cpp
#include "BaseNode/BatchData.h"

class OptimizedBatchData : public FrameworkData {
private:
    std::vector<std::shared_ptr<FrameworkData>> items_;
    std::shared_ptr<std::pmr::memory_resource> memory_pool_;
    
    // Pre-allocated buffer for small batches
    static constexpr size_t SMALL_BATCH_SIZE = 64;
    std::array<std::shared_ptr<FrameworkData>, SMALL_BATCH_SIZE> small_batch_buffer_;
    size_t small_batch_count_ = 0;
    
public:
    explicit OptimizedBatchData(size_t reserve_size = 0) {
        if (reserve_size > SMALL_BATCH_SIZE) {
            items_.reserve(reserve_size);
        }
    }
    
    void add_item(std::shared_ptr<FrameworkData> item) {
        if (!item) {
            throw std::invalid_argument("Cannot add null item to batch");
        }
        
        // Use small buffer for small batches
        if (items_.empty() && small_batch_count_ < SMALL_BATCH_SIZE) {
            small_batch_buffer_[small_batch_count_++] = std::move(item);
            return;
        }
        
        // Migrate to vector if needed
        if (!items_.empty() || small_batch_count_ >= SMALL_BATCH_SIZE) {
            if (items_.empty()) {
                items_.reserve(SMALL_BATCH_SIZE * 2);
                // Copy from small buffer
                for (size_t i = 0; i < small_batch_count_; ++i) {
                    items_.emplace_back(std::move(small_batch_buffer_[i]));
                }
                small_batch_count_ = 0;
            }
            items_.emplace_back(std::move(item));
        }
    }
    
    // Optimized iteration
    template<typename F>
    void for_each(F&& func) const {
        if (!items_.empty()) {
            std::for_each(items_.begin(), items_.end(), std::forward<F>(func));
        } else {
            for (size_t i = 0; i < small_batch_count_; ++i) {
                func(small_batch_buffer_[i]);
            }
        }
    }
    
    size_t size() const noexcept {
        return items_.empty() ? small_batch_count_ : items_.size();
    }
    
    bool empty() const noexcept {
        return items_.empty() && small_batch_count_ == 0;
    }
    
    // FrameworkData interface
    [[nodiscard]] std::unique_ptr<FrameworkData> clone() const override {
        auto result = std::make_unique<OptimizedBatchData>();
        
        for_each([&result](const std::shared_ptr<FrameworkData>& item) {
            result->add_item(item->clone());
        });
        
        return result;
    }
    
    [[nodiscard]] const std::type_info& type() const noexcept override {
        return typeid(OptimizedBatchData);
    }
};
```

### SIMD-Optimized Batch Processing

```cpp
#include <immintrin.h>  // For SIMD intrinsics

class SIMDOptimizedBatchNode : public BatchNode {
public:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData");
        }
        
        // Extract numeric data for SIMD processing
        std::vector<float> numeric_data;
        numeric_data.reserve(batch->data.size());
        
        for (const auto& item : batch->data) {
            if (auto* typed_item = item->as<TypedData<float>>()) {
                numeric_data.push_back(typed_item->get());
            }
        }
        
        // Process with SIMD if we have enough data
        if (numeric_data.size() >= 8) {
            process_with_simd(numeric_data);
        } else {
            process_scalar(numeric_data);
        }
        
        // Convert back to framework data
        auto result = std::make_shared<VectorData>();
        result->data.reserve(numeric_data.size());
        
        for (float value : numeric_data) {
            result->data.emplace_back(
                std::make_shared<TypedData<float>>(value)
            );
        }
        
        return result;
    }
    
private:
    void process_with_simd(std::vector<float>& data) {
        const size_t simd_size = 8;  // AVX processes 8 floats at once
        const size_t simd_iterations = data.size() / simd_size;
        
        // Process in SIMD chunks
        for (size_t i = 0; i < simd_iterations; ++i) {
            size_t offset = i * simd_size;
            
            // Load 8 floats
            __m256 values = _mm256_loadu_ps(&data[offset]);
            
            // Example operation: multiply by 2.0
            __m256 multiplier = _mm256_set1_ps(2.0f);
            __m256 result = _mm256_mul_ps(values, multiplier);
            
            // Store result back
            _mm256_storeu_ps(&data[offset], result);
        }
        
        // Process remaining elements with scalar operations
        for (size_t i = simd_iterations * simd_size; i < data.size(); ++i) {
            data[i] *= 2.0f;
        }
    }
    
    void process_scalar(std::vector<float>& data) {
        // Fallback scalar processing
        for (float& value : data) {
            value *= 2.0f;
        }
    }
};
```

## I/O and Network Optimization

### Async I/O with io_uring (Linux)

```cpp
#ifdef __linux__
#include <liburing.h>

class AsyncIONode : public AsyncNode {
private:
    struct io_uring ring_;
    static constexpr unsigned QUEUE_DEPTH = 64;
    
public:
    AsyncIONode() {
        // Initialize io_uring
        if (io_uring_queue_init(QUEUE_DEPTH, &ring_, 0) < 0) {
            throw std::runtime_error("Failed to initialize io_uring");
        }
    }
    
    ~AsyncIONode() {
        io_uring_queue_exit(&ring_);
    }
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [this, prep_res]() {
            if (auto* file_path = prep_res->as<StringData>()) {
                return read_file_async(file_path->value);
            }
            throw std::invalid_argument("Expected file path");
        });
    }
    
private:
    std::shared_ptr<FrameworkData> read_file_async(const std::string& file_path) {
        // Open file
        int fd = open(file_path.c_str(), O_RDONLY);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file: " + file_path);
        }
        
        // Get file size
        struct stat st;
        if (fstat(fd, &st) < 0) {
            close(fd);
            throw std::runtime_error("Failed to get file size");
        }
        
        // Allocate buffer
        std::string buffer(st.st_size, '\0');
        
        // Prepare read operation
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        io_uring_prep_read(sqe, fd, buffer.data(), buffer.size(), 0);
        
        // Submit and wait
        io_uring_submit(&ring_);
        
        struct io_uring_cqe* cqe;
        io_uring_wait_cqe(&ring_, &cqe);
        
        int result = cqe->res;
        io_uring_cqe_seen(&ring_, cqe);
        
        close(fd);
        
        if (result < 0) {
            throw std::runtime_error("Read failed");
        }
        
        buffer.resize(result);
        return std::make_shared<StringData>(std::move(buffer));
    }
};
#endif
```

### Connection Pooling

```cpp
#include <queue>
#include <mutex>
#include <condition_variable>

class ConnectionPool {
private:
    struct Connection {
        int socket_fd;
        std::chrono::steady_clock::time_point last_used;
        
        Connection(int fd) : socket_fd(fd), last_used(std::chrono::steady_clock::now()) {}
    };
    
    std::queue<std::unique_ptr<Connection>> available_connections_;
    std::mutex pool_mutex_;
    std::condition_variable pool_condition_;
    
    size_t max_connections_;
    size_t current_connections_ = 0;
    std::chrono::seconds connection_timeout_{300};  // 5 minutes
    
public:
    explicit ConnectionPool(size_t max_connections = 10) 
        : max_connections_(max_connections) {}
    
    std::unique_ptr<Connection> acquire_connection() {
        std::unique_lock<std::mutex> lock(pool_mutex_);
        
        // Clean up expired connections
        cleanup_expired_connections();
        
        // Try to get existing connection
        if (!available_connections_.empty()) {
            auto conn = std::move(available_connections_.front());
            available_connections_.pop();
            return conn;
        }
        
        // Create new connection if under limit
        if (current_connections_ < max_connections_) {
            auto conn = create_new_connection();
            if (conn) {
                ++current_connections_;
                return conn;
            }
        }
        
        // Wait for available connection
        pool_condition_.wait(lock, [this] {
            return !available_connections_.empty();
        });
        
        auto conn = std::move(available_connections_.front());
        available_connections_.pop();
        return conn;
    }
    
    void release_connection(std::unique_ptr<Connection> conn) {
        if (!conn) return;
        
        conn->last_used = std::chrono::steady_clock::now();
        
        std::lock_guard<std::mutex> lock(pool_mutex_);
        available_connections_.push(std::move(conn));
        pool_condition_.notify_one();
    }
    
private:
    std::unique_ptr<Connection> create_new_connection() {
        // Create actual network connection
        int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0) {
            return nullptr;
        }
        
        // Configure socket options for performance
        int flag = 1;
        setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        
        // Set non-blocking mode
        int flags = fcntl(socket_fd, F_GETFL, 0);
        fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
        
        return std::make_unique<Connection>(socket_fd);
    }
    
    void cleanup_expired_connections() {
        auto now = std::chrono::steady_clock::now();
        
        while (!available_connections_.empty()) {
            auto& conn = available_connections_.front();
            
            if (now - conn->last_used > connection_timeout_) {
                close(conn->socket_fd);
                available_connections_.pop();
                --current_connections_;
            } else {
                break;  // Connections are ordered by last_used
            }
        }
    }
};

class NetworkOptimizedNode : public AsyncNode {
private:
    ConnectionPool connection_pool_;
    
public:
    NetworkOptimizedNode() : connection_pool_(20) {}  // 20 max connections
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [this, prep_res]() {
            auto connection = connection_pool_.acquire_connection();
            
            try {
                auto result = perform_network_operation(connection.get(), prep_res);
                connection_pool_.release_connection(std::move(connection));
                return result;
            } catch (...) {
                // Connection might be bad, don't return to pool
                throw;
            }
        });
    }
    
private:
    std::shared_ptr<FrameworkData> perform_network_operation(
        ConnectionPool::Connection* conn, 
        std::shared_ptr<FrameworkData> data) {
        
        // Perform network operation using the connection
        // Implementation depends on specific protocol
        return data;
    }
};
```

## Profiling and Monitoring

### Performance Profiler Integration

```cpp
#include "BaseNode/PerformanceProfiler.h"

class ProfiledNode : public Node {
private:
    mutable PerformanceProfiler profiler_;
    
public:
    std::shared_ptr<FrameworkData> Execute(std::shared_ptr<FrameworkData> shared) override {
        auto timer = profiler_.start_timer("Execute");
        
        try {
            auto result = Node::Execute(shared);
            
            // Record successful execution
            profiler_.record_event("execution_success");
            
            return result;
        } catch (...) {
            // Record failed execution
            profiler_.record_event("execution_failure");
            throw;
        }
    }
    
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        auto timer = profiler_.start_timer("Exec");
        
        // Your processing logic here
        if (auto* input = prep_res->as<StringData>()) {
            // Simulate processing time
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            
            return std::make_shared<StringData>("Processed: " + input->value);
        }
        
        throw std::invalid_argument("Expected StringData");
    }
    
    // Get performance statistics
    PerformanceStats get_performance_stats() const {
        return profiler_.get_stats();
    }
};
```

### Custom Metrics Collection

```cpp
#include <atomic>
#include <chrono>

class MetricsCollector {
private:
    std::atomic<uint64_t> total_operations_{0};
    std::atomic<uint64_t> successful_operations_{0};
    std::atomic<uint64_t> failed_operations_{0};
    std::atomic<uint64_t> total_processing_time_us_{0};
    
    mutable std::mutex histogram_mutex_;
    std::map<uint64_t, uint64_t> latency_histogram_;  // microseconds -> count
    
public:
    void record_operation(std::chrono::microseconds duration, bool success) {
        total_operations_.fetch_add(1, std::memory_order_relaxed);
        
        if (success) {
            successful_operations_.fetch_add(1, std::memory_order_relaxed);
        } else {
            failed_operations_.fetch_add(1, std::memory_order_relaxed);
        }
        
        auto duration_us = duration.count();
        total_processing_time_us_.fetch_add(duration_us, std::memory_order_relaxed);
        
        // Update histogram (with bucketing for efficiency)
        uint64_t bucket = get_latency_bucket(duration_us);
        {
            std::lock_guard<std::mutex> lock(histogram_mutex_);
            histogram_[bucket]++;
        }
    }
    
    struct Metrics {
        uint64_t total_operations;
        uint64_t successful_operations;
        uint64_t failed_operations;
        double success_rate;
        double average_latency_us;
        uint64_t p50_latency_us;
        uint64_t p95_latency_us;
        uint64_t p99_latency_us;
    };
    
    Metrics get_metrics() const {
        Metrics metrics;
        metrics.total_operations = total_operations_.load(std::memory_order_relaxed);
        metrics.successful_operations = successful_operations_.load(std::memory_order_relaxed);
        metrics.failed_operations = failed_operations_.load(std::memory_order_relaxed);
        
        if (metrics.total_operations > 0) {
            metrics.success_rate = static_cast<double>(metrics.successful_operations) / metrics.total_operations;
            metrics.average_latency_us = static_cast<double>(
                total_processing_time_us_.load(std::memory_order_relaxed)
            ) / metrics.total_operations;
        } else {
            metrics.success_rate = 0.0;
            metrics.average_latency_us = 0.0;
        }
        
        // Calculate percentiles
        std::lock_guard<std::mutex> lock(histogram_mutex_);
        calculate_percentiles(metrics);
        
        return metrics;
    }
    
private:
    uint64_t get_latency_bucket(uint64_t latency_us) const {
        // Logarithmic bucketing for better distribution
        if (latency_us < 10) return 10;
        if (latency_us < 100) return 100;
        if (latency_us < 1000) return 1000;
        if (latency_us < 10000) return 10000;
        if (latency_us < 100000) return 100000;
        return 1000000;  // 1 second+
    }
    
    void calculate_percentiles(Metrics& metrics) const {
        if (histogram_.empty()) {
            metrics.p50_latency_us = metrics.p95_latency_us = metrics.p99_latency_us = 0;
            return;
        }
        
        uint64_t total_count = 0;
        for (const auto& [bucket, count] : histogram_) {
            total_count += count;
        }
        
        uint64_t p50_target = total_count * 50 / 100;
        uint64_t p95_target = total_count * 95 / 100;
        uint64_t p99_target = total_count * 99 / 100;
        
        uint64_t running_count = 0;
        for (const auto& [bucket, count] : histogram_) {
            running_count += count;
            
            if (metrics.p50_latency_us == 0 && running_count >= p50_target) {
                metrics.p50_latency_us = bucket;
            }
            if (metrics.p95_latency_us == 0 && running_count >= p95_target) {
                metrics.p95_latency_us = bucket;
            }
            if (metrics.p99_latency_us == 0 && running_count >= p99_target) {
                metrics.p99_latency_us = bucket;
                break;
            }
        }
    }
};

class MetricsEnabledNode : public Node {
private:
    mutable MetricsCollector metrics_;
    
public:
    std::shared_ptr<FrameworkData> Execute(std::shared_ptr<FrameworkData> shared) override {
        auto start_time = std::chrono::steady_clock::now();
        bool success = false;
        
        try {
            auto result = Node::Execute(shared);
            success = true;
            return result;
        } catch (...) {
            success = false;
            throw;
        } finally {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - start_time
            );
            metrics_.record_operation(duration, success);
        }
    }
    
    MetricsCollector::Metrics get_metrics() const {
        return metrics_.get_metrics();
    }
};
```

## Platform-Specific Optimizations

### CPU Cache Optimization

```cpp
#include <immintrin.h>

class CacheOptimizedNode : public BatchNode {
private:
    // Align data structures to cache line boundaries
    alignas(64) struct CacheAlignedData {
        std::array<float, 16> values;
        std::atomic<size_t> count{0};
    };
    
    std::vector<CacheAlignedData> cache_friendly_storage_;
    
public:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData");
        }
        
        // Process data in cache-friendly chunks
        const size_t chunk_size = 64;  // Process 64 items at a time
        auto result = std::make_shared<VectorData>();
        result->data.reserve(batch->data.size());
        
        for (size_t i = 0; i < batch->data.size(); i += chunk_size) {
            size_t end = std::min(i + chunk_size, batch->data.size());
            process_chunk(batch->data, i, end, *result);
            
            // Prefetch next chunk
            if (end < batch->data.size()) {
                prefetch_chunk(batch->data, end, std::min(end + chunk_size, batch->data.size()));
            }
        }
        
        return result;
    }
    
private:
    void process_chunk(const std::vector<std::shared_ptr<FrameworkData>>& input,
                      size_t start, size_t end, VectorData& output) {
        
        for (size_t i = start; i < end; ++i) {
            if (auto* item = input[i]->as<StringData>()) {
                auto processed = std::make_shared<StringData>(
                    "Processed: " + item->value
                );
                output.data.emplace_back(processed);
            }
        }
    }
    
    void prefetch_chunk(const std::vector<std::shared_ptr<FrameworkData>>& input,
                       size_t start, size_t end) {
        
        for (size_t i = start; i < end; ++i) {
            // Prefetch data into cache
            _mm_prefetch(reinterpret_cast<const char*>(input[i].get()), _MM_HINT_T0);
        }
    }
};
```

### NUMA-Aware Processing

```cpp
#ifdef __linux__
#include <numa.h>
#include <numaif.h>

class NUMAAwareNode : public AsyncNode {
private:
    int preferred_numa_node_;
    
public:
    NUMAAwareNode() {
        // Get current NUMA node
        preferred_numa_node_ = numa_node_of_cpu(sched_getcpu());
        
        // Bind memory allocation to this NUMA node
        numa_set_preferred(preferred_numa_node_);
    }
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [this, prep_res]() {
            // Bind thread to NUMA node
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            
            // Get CPUs for this NUMA node
            struct bitmask* numa_cpus = numa_allocate_cpumask();
            numa_node_to_cpus(preferred_numa_node_, numa_cpus);
            
            for (int cpu = 0; cpu < numa_num_possible_cpus(); ++cpu) {
                if (numa_bitmask_isbitset(numa_cpus, cpu)) {
                    CPU_SET(cpu, &cpuset);
                }
            }
            
            numa_free_cpumask(numa_cpus);
            
            // Set CPU affinity
            pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
            
            // Process data
            return process_on_numa_node(prep_res);
        });
    }
    
private:
    std::shared_ptr<FrameworkData> process_on_numa_node(
        std::shared_ptr<FrameworkData> data) {
        
        // Allocate memory on the preferred NUMA node
        void* numa_memory = numa_alloc_onnode(4096, preferred_numa_node_);
        
        try {
            // Use NUMA-local memory for processing
            auto result = process_with_numa_memory(data, numa_memory);
            numa_free(numa_memory, 4096);
            return result;
        } catch (...) {
            numa_free(numa_memory, 4096);
            throw;
        }
    }
    
    std::shared_ptr<FrameworkData> process_with_numa_memory(
        std::shared_ptr<FrameworkData> data, void* numa_memory) {
        
        // Use the NUMA-local memory for processing
        // Implementation depends on specific processing needs
        return data;
    }
};
#endif
```

## Benchmarking

### Comprehensive Benchmark Suite

```cpp
#include <benchmark/benchmark.h>
#include <random>

class BenchmarkNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* input = prep_res->as<StringData>()) {
            // Simulate processing
            std::string result = "Processed: " + input->value;
            return std::make_shared<StringData>(std::move(result));
        }
        throw std::invalid_argument("Expected StringData");
    }
};

// Benchmark single node processing
static void BM_SingleNodeProcessing(benchmark::State& state) {
    auto node = std::make_shared<BenchmarkNode>();
    auto input = std::make_shared<StringData>("test data");
    
    for (auto _ : state) {
        auto result = node->Execute(input);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SingleNodeProcessing);

// Benchmark batch processing
static void BM_BatchProcessing(benchmark::State& state) {
    auto batch_node = std::make_shared<BatchNode>();
    
    // Create batch data
    auto batch = std::make_shared<VectorData>();
    batch->data.reserve(state.range(0));
    
    for (int i = 0; i < state.range(0); ++i) {
        batch->data.emplace_back(
            std::make_shared<StringData>("item" + std::to_string(i))
        );
    }
    
    for (auto _ : state) {
        auto result = batch_node->Execute(batch);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(StringData));
}
BENCHMARK(BM_BatchProcessing)->Range(8, 8192);

// Benchmark async processing
static void BM_AsyncProcessing(benchmark::State& state) {
    auto async_node = std::make_shared<AsyncNode>();
    auto input = std::make_shared<StringData>("async test data");
    
    for (auto _ : state) {
        auto future = async_node->RunAsync(input);
        auto result = future.get();
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_AsyncProcessing);

// Benchmark memory allocation patterns
static void BM_MemoryAllocation(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<std::shared_ptr<StringData>> data;
        data.reserve(state.range(0));
        
        for (int i = 0; i < state.range(0); ++i) {
            data.emplace_back(std::make_shared<StringData>("data" + std::to_string(i)));
        }
        
        benchmark::DoNotOptimize(data);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_MemoryAllocation)->Range(8, 8192);

// Benchmark concurrent processing
static void BM_ConcurrentProcessing(benchmark::State& state) {
    const int num_threads = state.range(0);
    auto node = std::make_shared<BenchmarkNode>();
    
    for (auto _ : state) {
        std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
        futures.reserve(num_threads);
        
        // Launch concurrent operations
        for (int i = 0; i < num_threads; ++i) {
            auto input = std::make_shared<StringData>("concurrent data " + std::to_string(i));
            futures.emplace_back(
                std::async(std::launch::async, [&node, input]() {
                    return node->Execute(input);
                })
            );
        }
        
        // Wait for all to complete
        for (auto& future : futures) {
            auto result = future.get();
            benchmark::DoNotOptimize(result);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * num_threads);
}
BENCHMARK(BM_ConcurrentProcessing)->Range(1, std::thread::hardware_concurrency());

BENCHMARK_MAIN();
```

### Performance Testing Framework

```cpp
class PerformanceTestSuite {
private:
    struct TestResult {
        std::string test_name;
        std::chrono::nanoseconds average_duration;
        std::chrono::nanoseconds min_duration;
        std::chrono::nanoseconds max_duration;
        double throughput_ops_per_sec;
        size_t memory_usage_bytes;
    };
    
    std::vector<TestResult> results_;
    
public:
    template<typename NodeType, typename InputType>
    void run_performance_test(const std::string& test_name,
                            std::shared_ptr<NodeType> node,
                            std::shared_ptr<InputType> input,
                            size_t iterations = 10000) {
        
        std::vector<std::chrono::nanoseconds> durations;
        durations.reserve(iterations);
        
        // Warm up
        for (size_t i = 0; i < 100; ++i) {
            node->Execute(input);
        }
        
        // Measure memory before test
        size_t memory_before = get_memory_usage();
        
        // Run test
        for (size_t i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            auto result = node->Execute(input);
            auto end = std::chrono::high_resolution_clock::now();
            
            durations.push_back(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            );
            
            // Prevent optimization
            benchmark::DoNotOptimize(result);
        }
        
        // Measure memory after test
        size_t memory_after = get_memory_usage();
        
        // Calculate statistics
        TestResult result;
        result.test_name = test_name;
        result.average_duration = std::accumulate(durations.begin(), durations.end(), 
                                                std::chrono::nanoseconds{0}) / iterations;
        result.min_duration = *std::min_element(durations.begin(), durations.end());
        result.max_duration = *std::max_element(durations.begin(), durations.end());
        result.throughput_ops_per_sec = 1e9 / result.average_duration.count();
        result.memory_usage_bytes = memory_after - memory_before;
        
        results_.push_back(result);
    }
    
    void print_results() const {
        std::cout << std::setw(30) << "Test Name" 
                  << std::setw(15) << "Avg (ns)"
                  << std::setw(15) << "Min (ns)"
                  << std::setw(15) << "Max (ns)"
                  << std::setw(15) << "Ops/sec"
                  << std::setw(15) << "Memory (B)" << std::endl;
        
        std::cout << std::string(105, '-') << std::endl;
        
        for (const auto& result : results_) {
            std::cout << std::setw(30) << result.test_name
                      << std::setw(15) << result.average_duration.count()
                      << std::setw(15) << result.min_duration.count()
                      << std::setw(15) << result.max_duration.count()
                      << std::setw(15) << std::fixed << std::setprecision(0) << result.throughput_ops_per_sec
                      << std::setw(15) << result.memory_usage_bytes << std::endl;
        }
    }
    
private:
    size_t get_memory_usage() const {
        // Platform-specific memory usage measurement
#ifdef __linux__
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string label, value, unit;
                iss >> label >> value >> unit;
                return std::stoull(value) * 1024;  // Convert KB to bytes
            }
        }
#endif
        return 0;
    }
};
```

This performance tuning guide provides comprehensive strategies for optimizing the C++ Agentic Framework for high-throughput scenarios, covering memory management, concurrency, I/O optimization, and detailed benchmarking approaches.