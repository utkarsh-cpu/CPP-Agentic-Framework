#pragma once

#include <atomic>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include "Logger.h"

namespace AgenticFramework {

// Memory allocation information
struct AllocationInfo {
    void* ptr;
    size_t size;
    std::string file;
    int line;
    std::string function;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id thread_id;
    
    AllocationInfo(void* p, size_t s, std::string f = "", int l = 0, std::string func = "")
        : ptr(p), size(s), file(std::move(f)), line(l), function(std::move(func))
        , timestamp(std::chrono::system_clock::now())
        , thread_id(std::this_thread::get_id()) {}
};

// Memory usage statistics
struct MemoryStats {
    size_t total_allocated = 0;
    size_t total_deallocated = 0;
    size_t current_usage = 0;
    size_t peak_usage = 0;
    size_t allocation_count = 0;
    size_t deallocation_count = 0;
    
    [[nodiscard]] size_t getCurrentUsage() const noexcept { return current_usage; }
    [[nodiscard]] size_t getPeakUsage() const noexcept { return peak_usage; }
    [[nodiscard]] size_t getTotalAllocated() const noexcept { return total_allocated; }
    [[nodiscard]] size_t getTotalDeallocated() const noexcept { return total_deallocated; }
    [[nodiscard]] size_t getAllocationCount() const noexcept { return allocation_count; }
    [[nodiscard]] size_t getDeallocationCount() const noexcept { return deallocation_count; }
    [[nodiscard]] size_t getLeakedBytes() const noexcept { return total_allocated - total_deallocated; }
    [[nodiscard]] size_t getLeakedAllocations() const noexcept { return allocation_count - deallocation_count; }
};

// Memory pool tracking for specific allocators
class MemoryPool {
private:
    std::string name_;
    std::atomic<size_t> total_allocated_{0};
    std::atomic<size_t> total_deallocated_{0};
    std::atomic<size_t> current_usage_{0};
    std::atomic<size_t> peak_usage_{0};
    std::atomic<size_t> allocation_count_{0};
    std::atomic<size_t> deallocation_count_{0};
    std::unordered_map<void*, AllocationInfo> allocations_;
    mutable std::mutex mutex_;
    std::atomic<bool> track_allocations_{false};
    
public:
    explicit MemoryPool(std::string name) : name_(std::move(name)) {}
    
    void setTrackAllocations(bool track) noexcept { track_allocations_.store(track); }
    [[nodiscard]] bool isTrackingAllocations() const noexcept { return track_allocations_.load(); }
    
    void recordAllocation(void* ptr, size_t size, std::string_view file = "", 
                         int line = 0, std::string_view function = "");
    void recordDeallocation(void* ptr);
    
    [[nodiscard]] MemoryStats getStats() const noexcept { 
        MemoryStats result;
        result.total_allocated = total_allocated_.load();
        result.total_deallocated = total_deallocated_.load();
        result.current_usage = current_usage_.load();
        result.peak_usage = peak_usage_.load();
        result.allocation_count = allocation_count_.load();
        result.deallocation_count = deallocation_count_.load();
        return result;
    }
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    
    [[nodiscard]] std::vector<AllocationInfo> getActiveAllocations() const;
    [[nodiscard]] std::string generateLeakReport() const;
    void logLeakReport() const;
};

// Global memory tracker
class MemoryTracker {
private:
    std::unordered_map<std::string, std::unique_ptr<MemoryPool>> pools_;
    std::atomic<size_t> global_total_allocated_{0};
    std::atomic<size_t> global_total_deallocated_{0};
    std::atomic<size_t> global_current_usage_{0};
    std::atomic<size_t> global_peak_usage_{0};
    std::atomic<size_t> global_allocation_count_{0};
    std::atomic<size_t> global_deallocation_count_{0};
    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    
    static std::unique_ptr<MemoryTracker> instance_;
    static std::once_flag init_flag_;
    
    MemoryTracker() = default;
    
public:
    static MemoryTracker& getInstance();
    
    void setEnabled(bool enabled) noexcept;
    [[nodiscard]] bool isEnabled() const noexcept;
    
    MemoryPool& getPool(std::string_view name);
    void removePool(std::string_view name);
    
    void recordGlobalAllocation(size_t size) noexcept;
    void recordGlobalDeallocation(size_t size) noexcept;
    
    [[nodiscard]] MemoryStats getGlobalStats() const noexcept { 
        MemoryStats result;
        result.total_allocated = global_total_allocated_.load();
        result.total_deallocated = global_total_deallocated_.load();
        result.current_usage = global_current_usage_.load();
        result.peak_usage = global_peak_usage_.load();
        result.allocation_count = global_allocation_count_.load();
        result.deallocation_count = global_deallocation_count_.load();
        return result;
    }
    [[nodiscard]] std::vector<std::string> getPoolNames() const;
    
    [[nodiscard]] std::string generateGlobalReport() const;
    void logGlobalReport() const;
    
    // Check for memory leaks across all pools
    [[nodiscard]] bool hasMemoryLeaks() const;
    void logMemoryLeaks() const;
};

// RAII memory tracker for scoped tracking
class ScopedMemoryTracker {
private:
    std::string context_;
    size_t initial_usage_;
    std::chrono::high_resolution_clock::time_point start_time_;
    
public:
    explicit ScopedMemoryTracker(std::string context)
        : context_(std::move(context))
        , initial_usage_(MemoryTracker::getInstance().getGlobalStats().getCurrentUsage())
        , start_time_(std::chrono::high_resolution_clock::now()) {}
    
    ~ScopedMemoryTracker() {
        auto end_time = std::chrono::high_resolution_clock::now();
        (void)end_time; // Suppress unused variable warning
        
        size_t final_usage = MemoryTracker::getInstance().getGlobalStats().getCurrentUsage();
        int64_t memory_delta = static_cast<int64_t>(final_usage) - static_cast<int64_t>(initial_usage_);
        
        if (memory_delta != 0) {
            LOG_DEBUG("Memory delta in " + context_ + ": " + std::to_string(memory_delta) + " bytes");
        }
    }
};

// Custom allocator that integrates with memory tracking
template<typename T>
class TrackedAllocator {
private:
    std::string pool_name_;
    
public:
    using value_type = T;
    
    explicit TrackedAllocator(std::string pool_name = "default") 
        : pool_name_(std::move(pool_name)) {}
    
    template<typename U>
    TrackedAllocator(const TrackedAllocator<U>& other) 
        : pool_name_(other.pool_name_) {}
    
    T* allocate(size_t n) {
        size_t bytes = n * sizeof(T);
        T* ptr = static_cast<T*>(std::malloc(bytes));
        
        if (!ptr) {
            throw std::bad_alloc();
        }
        
        if (MemoryTracker::getInstance().isEnabled()) {
            auto& pool = MemoryTracker::getInstance().getPool(pool_name_);
            pool.recordAllocation(ptr, bytes);
            MemoryTracker::getInstance().recordGlobalAllocation(bytes);
        }
        
        return ptr;
    }
    
    void deallocate(T* ptr, size_t n) {
        if (MemoryTracker::getInstance().isEnabled()) {
            size_t bytes = n * sizeof(T);
            auto& pool = MemoryTracker::getInstance().getPool(pool_name_);
            pool.recordDeallocation(ptr);
            MemoryTracker::getInstance().recordGlobalDeallocation(bytes);
        }
        
        std::free(ptr);
    }
    
    template<typename U>
    bool operator==(const TrackedAllocator<U>& other) const {
        return pool_name_ == other.pool_name_;
    }
    
    template<typename U>
    bool operator!=(const TrackedAllocator<U>& other) const {
        return !(*this == other);
    }
};

} // namespace AgenticFramework

// Convenience macros for memory tracking
#define TRACK_MEMORY_SCOPE(context) \
    AgenticFramework::ScopedMemoryTracker _mem_tracker(context)

#define TRACK_MEMORY_FUNCTION() \
    AgenticFramework::ScopedMemoryTracker _mem_tracker(__FUNCTION__)

// Helper functions for memory tracking integration
#define RECORD_ALLOCATION(ptr, size) \
    do { \
        if (AgenticFramework::MemoryTracker::getInstance().isEnabled()) { \
            auto& pool = AgenticFramework::MemoryTracker::getInstance().getPool("global"); \
            pool.recordAllocation(ptr, size, __FILE__, __LINE__, __FUNCTION__); \
        } \
    } while(0)

#define RECORD_DEALLOCATION(ptr) \
    do { \
        if (AgenticFramework::MemoryTracker::getInstance().isEnabled()) { \
            auto& pool = AgenticFramework::MemoryTracker::getInstance().getPool("global"); \
            pool.recordDeallocation(ptr); \
        } \
    } while(0)