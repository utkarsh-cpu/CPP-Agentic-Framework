#ifndef OPTIMIZEDBATCHPROCESSOR_H
#define OPTIMIZEDBATCHPROCESSOR_H

#include "FrameworkData.h"
#include "BatchData.h"
#include "Result.h"
#include "FrameworkError.h"

// Use the framework namespace for Result and FrameworkError
using framework::Result;
using framework::FrameworkError;
using framework::ErrorCategory;
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <functional>
#include <memory>
#include <chrono>
#include <algorithm>
#include <numeric>
// Architecture-specific SIMD includes
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    #include <immintrin.h> // For x86/x64 SIMD operations
    #define SIMD_X86_AVAILABLE 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #include <arm_neon.h> // For ARM NEON SIMD operations
    #define SIMD_ARM_AVAILABLE 1
#else
    #define SIMD_FALLBACK_ONLY 1
#endif
#include <type_traits>

// Forward declarations
template<typename T>
class ObjectPool;
class WorkStealingQueue;
class ThreadPool;

// Work item for batch processing operations
struct WorkItem {
    std::function<std::shared_ptr<FrameworkData>(std::shared_ptr<FrameworkData>)> processor;
    std::shared_ptr<FrameworkData> data;
    std::promise<std::shared_ptr<FrameworkData>> result_promise;
    std::chrono::steady_clock::time_point created_at;
    size_t priority = 0;
    
    WorkItem() = default;
    
    WorkItem(std::function<std::shared_ptr<FrameworkData>(std::shared_ptr<FrameworkData>)> proc,
             std::shared_ptr<FrameworkData> input_data,
             size_t work_priority = 0)
        : processor(std::move(proc))
        , data(std::move(input_data))
        , created_at(std::chrono::steady_clock::now())
        , priority(work_priority) {}
    
    // Move constructor and assignment
    WorkItem(WorkItem&& other) noexcept
        : processor(std::move(other.processor))
        , data(std::move(other.data))
        , result_promise(std::move(other.result_promise))
        , created_at(other.created_at)
        , priority(other.priority) {}
    
    WorkItem& operator=(WorkItem&& other) noexcept {
        if (this != &other) {
            processor = std::move(other.processor);
            data = std::move(other.data);
            result_promise = std::move(other.result_promise);
            created_at = other.created_at;
            priority = other.priority;
        }
        return *this;
    }
    
    // Disable copy operations
    WorkItem(const WorkItem&) = delete;
    WorkItem& operator=(const WorkItem&) = delete;
};

// Object pool for efficient WorkItem allocation
template<typename T>
class ObjectPool {
private:
    std::queue<std::unique_ptr<T>> pool_;
    mutable std::mutex mutex_;
    std::atomic<size_t> created_count_{0};
    std::atomic<size_t> reused_count_{0};
    const size_t max_pool_size_;
    
public:
    explicit ObjectPool(size_t max_size = 1000) : max_pool_size_(max_size) {}
    
    template<typename... Args>
    std::unique_ptr<T> acquire(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!pool_.empty()) {
            auto obj = std::move(pool_.front());
            pool_.pop();
            reused_count_.fetch_add(1, std::memory_order_relaxed);
            
            // Reset the object if it's a WorkItem
            if constexpr (std::is_same_v<T, WorkItem>) {
                *obj = WorkItem(std::forward<Args>(args)...);
            }
            return obj;
        }
        
        created_count_.fetch_add(1, std::memory_order_relaxed);
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
    
    void release(std::unique_ptr<T> obj) {
        if (!obj) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.size() < max_pool_size_) {
            pool_.push(std::move(obj));
        }
        // If pool is full, let the object be destroyed
    }
    
    // Statistics
    [[nodiscard]] size_t created_count() const noexcept {
        return created_count_.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] size_t reused_count() const noexcept {
        return reused_count_.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] size_t pool_size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!pool_.empty()) {
            pool_.pop();
        }
    }
};

// Work-stealing queue for load balancing
class WorkStealingQueue {
private:
    std::deque<std::unique_ptr<WorkItem>> queue_;
    mutable std::mutex mutex_;
    
public:
    void push(std::unique_ptr<WorkItem> item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(item));
    }
    
    std::unique_ptr<WorkItem> pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return nullptr;
        }
        auto item = std::move(queue_.back());
        queue_.pop_back();
        return item;
    }
    
    std::unique_ptr<WorkItem> steal() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return nullptr;
        }
        auto item = std::move(queue_.front());
        queue_.pop_front();
        return item;
    }
    
    [[nodiscard]] bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
};

// High-performance thread pool with work stealing
class ThreadPool {
private:
    std::vector<std::thread> workers_;
    std::vector<std::unique_ptr<WorkStealingQueue>> work_queues_;
    std::atomic<bool> stop_flag_{false};
    std::atomic<size_t> next_queue_{0};
    
    // Statistics
    std::atomic<size_t> tasks_completed_{0};
    std::atomic<size_t> tasks_stolen_{0};
    
    void worker_thread(size_t thread_id) {
        auto& local_queue = *work_queues_[thread_id];
        
        while (!stop_flag_.load(std::memory_order_relaxed)) {
            std::unique_ptr<WorkItem> work_item = nullptr;
            
            // Try to get work from local queue first
            work_item = local_queue.pop();
            
            // If no local work, try to steal from other queues
            if (!work_item) {
                for (size_t i = 0; i < work_queues_.size(); ++i) {
                    size_t target = (thread_id + i + 1) % work_queues_.size();
                    work_item = work_queues_[target]->steal();
                    if (work_item) {
                        tasks_stolen_.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                }
            }
            
            if (work_item) {
                try {
                    auto result = work_item->processor(work_item->data);
                    work_item->result_promise.set_value(result);
                    tasks_completed_.fetch_add(1, std::memory_order_relaxed);
                } catch (...) {
                    work_item->result_promise.set_exception(std::current_exception());
                }
            } else {
                // No work available, sleep briefly to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
    
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) {
        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
        }
        
        work_queues_.reserve(num_threads);
        workers_.reserve(num_threads);
        
        // Create work queues
        for (size_t i = 0; i < num_threads; ++i) {
            work_queues_.emplace_back(std::make_unique<WorkStealingQueue>());
        }
        
        // Create worker threads
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back(&ThreadPool::worker_thread, this, i);
        }
    }
    
    ~ThreadPool() {
        stop_flag_.store(true, std::memory_order_relaxed);
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    // Disable copy and move operations
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    template<typename Processor>
    std::future<std::shared_ptr<FrameworkData>> submit(
        Processor&& processor, 
        std::shared_ptr<FrameworkData> data,
        size_t priority = 0) {
        
        auto work_item = std::make_unique<WorkItem>(
            std::forward<Processor>(processor), 
            std::move(data), 
            priority
        );
        
        auto future = work_item->result_promise.get_future();
        
        // Use round-robin to distribute work
        size_t queue_index = next_queue_.fetch_add(1, std::memory_order_relaxed) % work_queues_.size();
        work_queues_[queue_index]->push(std::move(work_item));
        
        return future;
    }
    
    // Statistics
    [[nodiscard]] size_t thread_count() const noexcept {
        return workers_.size();
    }
    
    [[nodiscard]] size_t tasks_completed() const noexcept {
        return tasks_completed_.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] size_t tasks_stolen() const noexcept {
        return tasks_stolen_.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] size_t total_queued_tasks() const {
        size_t total = 0;
        for (const auto& queue : work_queues_) {
            total += queue->size();
        }
        return total;
    }
};

// SIMD-optimized operations for numeric data processing
namespace SIMDOps {
    // Forward declarations
    void add_arrays_simd(const float* a, const float* b, float* result, size_t size);
    void multiply_arrays_simd(const float* a, const float* b, float* result, size_t size);
    float sum_array_simd(const float* array, size_t size);
    
    // Fallback implementations for systems without SIMD support
    inline void add_arrays_fallback(const float* a, const float* b, float* result, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }
    
    inline void multiply_arrays_fallback(const float* a, const float* b, float* result, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            result[i] = a[i] * b[i];
        }
    }
    
    inline float sum_array_fallback(const float* array, size_t size) {
        float sum = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            sum += array[i];
        }
        return sum;
    }
    
    // Check if SIMD is supported
    bool is_simd_supported();
    
    // Safe SIMD operations that fall back to scalar if SIMD is not available
    inline void add_arrays_safe(const float* a, const float* b, float* result, size_t size) {
        if (is_simd_supported() && size >= 4) {
            add_arrays_simd(a, b, result, size);
        } else {
            add_arrays_fallback(a, b, result, size);
        }
    }
    
    inline void multiply_arrays_safe(const float* a, const float* b, float* result, size_t size) {
        if (is_simd_supported() && size >= 4) {
            multiply_arrays_simd(a, b, result, size);
        } else {
            multiply_arrays_fallback(a, b, result, size);
        }
    }
    
    inline float sum_array_safe(const float* array, size_t size) {
        if (is_simd_supported() && size >= 4) {
            return sum_array_simd(array, size);
        } else {
            return sum_array_fallback(array, size);
        }
    }
}

// Main optimized batch processor class
class OptimizedBatchProcessor {
private:
    std::unique_ptr<ThreadPool> thread_pool_;
    ObjectPool<WorkItem> work_item_pool_;
    
    // Configuration
    size_t min_parallel_threshold_;
    bool enable_simd_;
    
    // Statistics
    std::atomic<size_t> batches_processed_{0};
    std::atomic<size_t> items_processed_{0};
    std::atomic<std::chrono::nanoseconds> total_processing_time_{std::chrono::nanoseconds{0}};
    
public:
    struct Config {
        size_t thread_count = std::thread::hardware_concurrency();
        size_t min_parallel_threshold = 100;
        bool enable_simd = true;
        size_t work_item_pool_size = 1000;
    };
    
    explicit OptimizedBatchProcessor(const Config& config)
        : thread_pool_(std::make_unique<ThreadPool>(config.thread_count))
        , work_item_pool_(config.work_item_pool_size)
        , min_parallel_threshold_(config.min_parallel_threshold)
        , enable_simd_(config.enable_simd) {}
    
    // Default constructor using default config
    OptimizedBatchProcessor() : OptimizedBatchProcessor(Config{}) {}
    
    ~OptimizedBatchProcessor() = default;
    
    // Disable copy and move operations
    OptimizedBatchProcessor(const OptimizedBatchProcessor&) = delete;
    OptimizedBatchProcessor& operator=(const OptimizedBatchProcessor&) = delete;
    OptimizedBatchProcessor(OptimizedBatchProcessor&&) = delete;
    OptimizedBatchProcessor& operator=(OptimizedBatchProcessor&&) = delete;
    
    // Main batch processing method
    template<typename Processor>
    Result<std::unique_ptr<BatchData>> process_batch(
        const BatchData& input_batch,
        Processor&& processor) {
        
        [[maybe_unused]] auto start_time = std::chrono::steady_clock::now();
        
        try {
            if (input_batch.empty()) {
                return Result<std::unique_ptr<BatchData>>(
                    FrameworkError("Input batch is empty", ErrorCategory::VALIDATION_ERROR)
                );
            }
            
            auto result_batch = std::make_unique<BatchData>(input_batch.size());
            
            // For small batches, process sequentially
            if (input_batch.size() < min_parallel_threshold_) {
                return process_sequential(input_batch, std::forward<Processor>(processor));
            }
            
            // For large batches, process in parallel
            return process_parallel(input_batch, std::forward<Processor>(processor));
            
        } catch (const std::exception& e) {
            return Result<std::unique_ptr<BatchData>>(
                FrameworkError("Batch processing failed: " + std::string(e.what()), 
                              ErrorCategory::RUNTIME_ERROR)
            );
        }
    }
    
    // Specialized SIMD-optimized numeric processing
    template<typename NumericType>
    Result<std::unique_ptr<BatchData>> process_numeric_batch_simd(
        const BatchData& input_batch,
        std::function<NumericType(NumericType)> processor) {
        
        static_assert(std::is_arithmetic_v<NumericType>, 
                     "SIMD processing only supports arithmetic types");
        
        if (!enable_simd_) {
            return process_batch(input_batch, [&processor](auto data) -> std::shared_ptr<FrameworkData> {
                if (auto typed_data = data->template as<TypedData<NumericType>>()) {
                    auto result = processor(typed_data->get());
                    return std::make_shared<TypedData<NumericType>>(result);
                }
                return nullptr;
            });
        }
        
        // SIMD-optimized implementation for supported types
        if constexpr (std::is_same_v<NumericType, float>) {
            return process_float_batch_simd(input_batch, processor);
        } else {
            // Fall back to regular parallel processing for unsupported types
            return process_batch(input_batch, [&processor](auto data) -> std::shared_ptr<FrameworkData> {
                if (auto typed_data = data->template as<TypedData<NumericType>>()) {
                    auto result = processor(typed_data->get());
                    return std::make_shared<TypedData<NumericType>>(result);
                }
                return nullptr;
            });
        }
    }
    
    // Statistics and monitoring
    [[nodiscard]] size_t batches_processed() const noexcept {
        return batches_processed_.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] size_t items_processed() const noexcept {
        return items_processed_.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] double average_processing_time_ms() const noexcept {
        auto total_time = total_processing_time_.load(std::memory_order_relaxed);
        auto batch_count = batches_processed_.load(std::memory_order_relaxed);
        if (batch_count == 0) return 0.0;
        
        return std::chrono::duration<double, std::milli>(total_time).count() / batch_count;
    }
    
    [[nodiscard]] size_t thread_count() const noexcept {
        return thread_pool_->thread_count();
    }
    
    [[nodiscard]] size_t work_stealing_count() const noexcept {
        return thread_pool_->tasks_stolen();
    }
    
    [[nodiscard]] size_t work_item_pool_efficiency() const noexcept {
        auto created = work_item_pool_.created_count();
        auto reused = work_item_pool_.reused_count();
        if (created + reused == 0) return 100;
        return (reused * 100) / (created + reused);
    }

private:
    template<typename Processor>
    Result<std::unique_ptr<BatchData>> process_sequential(
        const BatchData& input_batch,
        Processor&& processor) {
        
        auto result_batch = std::make_unique<BatchData>(input_batch.size());
        
        for (const auto& item : input_batch) {
            if (!item) {
                return Result<std::unique_ptr<BatchData>>(
                    FrameworkError("Null item found in batch", ErrorCategory::VALIDATION_ERROR)
                );
            }
            
            try {
                auto result = processor(item);
                result_batch->add_item(result);
            } catch (const std::exception& e) {
                return Result<std::unique_ptr<BatchData>>(
                    FrameworkError("Item processing failed: " + std::string(e.what()), 
                                  ErrorCategory::RUNTIME_ERROR)
                );
            }
        }
        
        // Update statistics
        batches_processed_.fetch_add(1, std::memory_order_relaxed);
        items_processed_.fetch_add(input_batch.size(), std::memory_order_relaxed);
        
        return Result<std::unique_ptr<BatchData>>(std::move(result_batch));
    }
    
    template<typename Processor>
    Result<std::unique_ptr<BatchData>> process_parallel(
        const BatchData& input_batch,
        Processor&& processor) {
        
        std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
        futures.reserve(input_batch.size());
        
        // Submit all work items to thread pool
        for (const auto& item : input_batch) {
            if (!item) {
                return Result<std::unique_ptr<BatchData>>(
                    FrameworkError("Null item found in batch", ErrorCategory::VALIDATION_ERROR)
                );
            }
            
            futures.emplace_back(
                thread_pool_->submit(
                    [processor](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
                        return processor(data);
                    },
                    item
                )
            );
        }
        
        // Collect results
        auto result_batch = std::make_unique<BatchData>(input_batch.size());
        
        try {
            for (auto& future : futures) {
                auto result = future.get();
                result_batch->add_item(result);
            }
        } catch (const std::exception& e) {
            return Result<std::unique_ptr<BatchData>>(
                FrameworkError("Parallel processing failed: " + std::string(e.what()), 
                              ErrorCategory::RUNTIME_ERROR)
            );
        }
        
        // Update statistics
        batches_processed_.fetch_add(1, std::memory_order_relaxed);
        items_processed_.fetch_add(input_batch.size(), std::memory_order_relaxed);
        
        return Result<std::unique_ptr<BatchData>>(std::move(result_batch));
    }
    
    Result<std::unique_ptr<BatchData>> process_float_batch_simd(
        const BatchData& input_batch,
        std::function<float(float)> processor) {
        
        // Extract float values for SIMD processing
        std::vector<float> input_values;
        input_values.reserve(input_batch.size());
        
        for (const auto& item : input_batch) {
            if (auto typed_data = item->template as<TypedData<float>>()) {
                input_values.push_back(typed_data->get());
            } else {
                return Result<std::unique_ptr<BatchData>>(
                    FrameworkError("Non-float item found in SIMD batch", ErrorCategory::VALIDATION_ERROR)
                );
            }
        }
        
        // Process with SIMD if applicable (for simple operations)
        std::vector<float> output_values(input_values.size());
        
        // For now, fall back to regular processing
        // In a real implementation, you'd detect if the processor can be vectorized
        for (size_t i = 0; i < input_values.size(); ++i) {
            output_values[i] = processor(input_values[i]);
        }
        
        // Convert back to BatchData
        auto result_batch = std::make_unique<BatchData>(output_values.size());
        for (float value : output_values) {
            result_batch->add_typed_item(value);
        }
        
        return Result<std::unique_ptr<BatchData>>(std::move(result_batch));
    }
};

// Factory functions for creating optimized processors
std::unique_ptr<OptimizedBatchProcessor> create_optimized_processor(
    size_t thread_count = 0,
    bool enable_simd = true);

std::unique_ptr<OptimizedBatchProcessor> create_high_performance_processor();

std::unique_ptr<OptimizedBatchProcessor> create_memory_efficient_processor();

// Specialized batch processing functions
namespace SpecializedProcessors {
    // Numeric aggregation processor
    template<typename T>
    Result<T> aggregate_numeric_batch(
        const BatchData& batch,
        std::function<T(T, T)> aggregator,
        T initial_value,
        OptimizedBatchProcessor& /* processor */) {
        
        static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");
        
        if (batch.empty()) {
            return Result<T>(FrameworkError("Empty batch for aggregation", ErrorCategory::VALIDATION_ERROR));
        }
        
        // Extract values
        std::vector<T> values;
        values.reserve(batch.size());
        
        for (const auto& item : batch) {
            if (auto typed_data = item->template as<TypedData<T>>()) {
                values.push_back(typed_data->get());
            } else {
                return Result<T>(FrameworkError("Invalid data type in batch", ErrorCategory::VALIDATION_ERROR));
            }
        }
        
        // Use SIMD for supported operations
        if constexpr (std::is_same_v<T, float>) {
            // Special case for sum aggregation
            if (values.size() >= 4) {
                float result = SIMDOps::sum_array_safe(values.data(), values.size());
                return Result<T>(static_cast<T>(result));
            }
        }
        
        // Fallback to standard aggregation
        T result = initial_value;
        for (const T& value : values) {
            result = aggregator(result, value);
        }
        
        return Result<T>(result);
    }
    
    // Parallel map operation
    template<typename InputType, typename OutputType, typename Processor>
    Result<std::unique_ptr<BatchData>> parallel_map(
        const BatchData& input_batch,
        Processor&& processor,
        OptimizedBatchProcessor& batch_processor) {
        
        return batch_processor.process_batch(input_batch, 
            [&processor](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
                if (auto typed_data = data->template as<TypedData<InputType>>()) {
                    auto result = processor(typed_data->get());
                    return std::make_shared<TypedData<OutputType>>(result);
                }
                return nullptr;
            });
    }
    
    // Parallel filter operation
    template<typename T, typename Predicate>
    Result<std::unique_ptr<BatchData>> parallel_filter(
        const BatchData& input_batch,
        Predicate&& predicate,
        OptimizedBatchProcessor& batch_processor) {
        
        // First pass: determine which items pass the filter
        auto filter_processor = [&predicate](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
            if (auto typed_data = data->template as<TypedData<T>>()) {
                bool passes = predicate(typed_data->get());
                return std::make_shared<TypedData<bool>>(passes);
            }
            return std::make_shared<TypedData<bool>>(false);
        };
        
        auto filter_result = batch_processor.process_batch(input_batch, filter_processor);
        if (!filter_result.has_value()) {
            return Result<std::unique_ptr<BatchData>>(filter_result.error());
        }
        
        // Second pass: collect items that passed the filter
        auto filtered_batch = std::make_unique<BatchData>();
        auto& filter_batch = filter_result.value();
        
        for (size_t i = 0; i < input_batch.size() && i < filter_batch->size(); ++i) {
            if (auto bool_data = filter_batch->at(i)->template as<TypedData<bool>>()) {
                if (bool_data->get()) {
                    filtered_batch->add_item(input_batch.at(i));
                }
            }
        }
        
        return Result<std::unique_ptr<BatchData>>(std::move(filtered_batch));
    }
}

#endif // OPTIMIZEDBATCHPROCESSOR_H