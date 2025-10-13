#ifndef MEMORY_PROFILER_H
#define MEMORY_PROFILER_H

#include <chrono>
#include <memory>
#include <atomic>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

// Simple memory profiling utility for batch processing performance tests
class MemoryProfiler {
public:
    struct AllocationInfo {
        size_t size;
        std::chrono::high_resolution_clock::time_point timestamp;
        std::string location;
    };
    
    struct MemoryStats {
        size_t total_allocations = 0;
        size_t total_deallocations = 0;
        size_t current_allocations = 0;
        size_t peak_allocations = 0;
        size_t total_bytes_allocated = 0;
        size_t total_bytes_deallocated = 0;
        size_t current_bytes = 0;
        size_t peak_bytes = 0;
        std::chrono::high_resolution_clock::time_point start_time;
        std::chrono::high_resolution_clock::time_point end_time;
    };
    
private:
    static std::atomic<bool> enabled_;
    static std::atomic<size_t> allocation_count_;
    static std::atomic<size_t> deallocation_count_;
    static std::atomic<size_t> current_bytes_;
    static std::atomic<size_t> peak_bytes_;
    static std::atomic<size_t> total_bytes_allocated_;
    static std::atomic<size_t> total_bytes_deallocated_;
    static std::chrono::high_resolution_clock::time_point start_time_;
    
public:
    // Enable/disable profiling
    static void enable() {
        enabled_ = true;
        reset();
    }
    
    static void disable() {
        enabled_ = false;
    }
    
    static bool is_enabled() {
        return enabled_;
    }
    
    // Reset counters
    static void reset() {
        allocation_count_ = 0;
        deallocation_count_ = 0;
        current_bytes_ = 0;
        peak_bytes_ = 0;
        total_bytes_allocated_ = 0;
        total_bytes_deallocated_ = 0;
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    
    // Record allocation
    static void record_allocation(size_t size, const std::string& location = "") {
        (void)location; // Suppress unused parameter warning
        if (!enabled_) return;
        
        allocation_count_++;
        total_bytes_allocated_ += size;
        current_bytes_ += size;
        
        size_t current = current_bytes_.load();
        size_t peak = peak_bytes_.load();
        while (current > peak && !peak_bytes_.compare_exchange_weak(peak, current)) {
            peak = peak_bytes_.load();
        }
    }
    
    // Record deallocation
    static void record_deallocation(size_t size) {
        if (!enabled_) return;
        
        deallocation_count_++;
        total_bytes_deallocated_ += size;
        current_bytes_ -= size;
    }
    
    // Get current statistics
    static MemoryStats get_stats() {
        MemoryStats stats;
        stats.total_allocations = allocation_count_;
        stats.total_deallocations = deallocation_count_;
        stats.current_allocations = stats.total_allocations - stats.total_deallocations;
        stats.peak_allocations = allocation_count_; // Simplified
        stats.total_bytes_allocated = total_bytes_allocated_;
        stats.total_bytes_deallocated = total_bytes_deallocated_;
        stats.current_bytes = current_bytes_;
        stats.peak_bytes = peak_bytes_;
        stats.start_time = start_time_;
        stats.end_time = std::chrono::high_resolution_clock::now();
        return stats;
    }
    
    // Print statistics
    static void print_stats(const std::string& label = "") {
        auto stats = get_stats();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            stats.end_time - stats.start_time);
        
        std::cout << "\n=== Memory Profile" << (label.empty() ? "" : " (" + label + ")") << " ===\n";
        std::cout << "Duration: " << duration.count() << " ms\n";
        std::cout << "Total allocations: " << stats.total_allocations << "\n";
        std::cout << "Total deallocations: " << stats.total_deallocations << "\n";
        std::cout << "Current allocations: " << stats.current_allocations << "\n";
        std::cout << "Total bytes allocated: " << stats.total_bytes_allocated << " bytes\n";
        std::cout << "Total bytes deallocated: " << stats.total_bytes_deallocated << " bytes\n";
        std::cout << "Current bytes: " << stats.current_bytes << " bytes\n";
        std::cout << "Peak bytes: " << stats.peak_bytes << " bytes\n";
        
        if (stats.total_allocations > 0) {
            std::cout << "Average allocation size: " 
                      << (stats.total_bytes_allocated / stats.total_allocations) << " bytes\n";
        }
        
        std::cout << "=======================================\n\n";
    }
    
    // Save statistics to file
    static void save_stats_to_file(const std::string& filename, const std::string& label = "") {
        auto stats = get_stats();
        std::ofstream file(filename, std::ios::app);
        
        if (file.is_open()) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                stats.end_time - stats.start_time);
            
            file << label << "," 
                 << duration.count() << ","
                 << stats.total_allocations << ","
                 << stats.total_deallocations << ","
                 << stats.current_allocations << ","
                 << stats.total_bytes_allocated << ","
                 << stats.total_bytes_deallocated << ","
                 << stats.current_bytes << ","
                 << stats.peak_bytes << "\n";
        }
    }
};

// RAII helper for scoped profiling
class ScopedMemoryProfiler {
private:
    std::string label_;
    bool was_enabled_;
    
public:
    explicit ScopedMemoryProfiler(const std::string& label) : label_(label) {
        was_enabled_ = MemoryProfiler::is_enabled();
        if (!was_enabled_) {
            MemoryProfiler::enable();
        } else {
            MemoryProfiler::reset();
        }
    }
    
    ~ScopedMemoryProfiler() {
        MemoryProfiler::print_stats(label_);
        if (!was_enabled_) {
            MemoryProfiler::disable();
        }
    }
};

// Static member definitions
std::atomic<bool> MemoryProfiler::enabled_{false};
std::atomic<size_t> MemoryProfiler::allocation_count_{0};
std::atomic<size_t> MemoryProfiler::deallocation_count_{0};
std::atomic<size_t> MemoryProfiler::current_bytes_{0};
std::atomic<size_t> MemoryProfiler::peak_bytes_{0};
std::atomic<size_t> MemoryProfiler::total_bytes_allocated_{0};
std::atomic<size_t> MemoryProfiler::total_bytes_deallocated_{0};
std::chrono::high_resolution_clock::time_point MemoryProfiler::start_time_;

// Utility macros for easy profiling
#define PROFILE_MEMORY(label) ScopedMemoryProfiler _prof(label)
#define RECORD_ALLOCATION(size) MemoryProfiler::record_allocation(size, __FUNCTION__)
#define RECORD_DEALLOCATION(size) MemoryProfiler::record_deallocation(size)

// Performance measurement utilities
class PerformanceTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::string label_;
    
public:
    explicit PerformanceTimer(const std::string& label) : label_(label) {
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    
    ~PerformanceTimer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
        std::cout << label_ << " took: " << duration.count() << " μs\n";
    }
    
    // Get elapsed time without destroying the timer
    std::chrono::microseconds elapsed() const {
        auto current_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(current_time - start_time_);
    }
};

#define TIME_OPERATION(label) PerformanceTimer _timer(label)

// Batch operation performance analyzer
class BatchPerformanceAnalyzer {
public:
    struct BatchMetrics {
        size_t batch_size;
        std::chrono::microseconds creation_time;
        std::chrono::microseconds processing_time;
        std::chrono::microseconds cleanup_time;
        size_t memory_used;
        size_t peak_memory;
        double items_per_second;
        double memory_efficiency;
    };
    
    template<typename CreateFunc, typename ProcessFunc, typename CleanupFunc>
    static BatchMetrics analyze_batch_operation(
        size_t batch_size,
        CreateFunc create_func,
        ProcessFunc process_func,
        CleanupFunc cleanup_func) {
        
        BatchMetrics metrics;
        metrics.batch_size = batch_size;
        
        // Measure creation
        MemoryProfiler::reset();
        MemoryProfiler::enable();
        
        auto start = std::chrono::high_resolution_clock::now();
        auto batch = create_func(batch_size);
        auto end = std::chrono::high_resolution_clock::now();
        metrics.creation_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        auto stats_after_creation = MemoryProfiler::get_stats();
        metrics.memory_used = stats_after_creation.current_bytes;
        
        // Measure processing
        start = std::chrono::high_resolution_clock::now();
        process_func(batch);
        end = std::chrono::high_resolution_clock::now();
        metrics.processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        auto stats_after_processing = MemoryProfiler::get_stats();
        metrics.peak_memory = stats_after_processing.peak_bytes;
        
        // Measure cleanup
        start = std::chrono::high_resolution_clock::now();
        cleanup_func(batch);
        end = std::chrono::high_resolution_clock::now();
        metrics.cleanup_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        MemoryProfiler::disable();
        
        // Calculate derived metrics
        if (metrics.processing_time.count() > 0) {
            metrics.items_per_second = (batch_size * 1000000.0) / metrics.processing_time.count();
        } else {
            metrics.items_per_second = 0.0;
        }
        
        if (metrics.peak_memory > 0) {
            metrics.memory_efficiency = static_cast<double>(metrics.memory_used) / metrics.peak_memory;
        } else {
            metrics.memory_efficiency = 1.0;
        }
        
        return metrics;
    }
    
    static void print_metrics(const BatchMetrics& metrics, const std::string& label = "") {
        std::cout << "\n=== Batch Performance Analysis" 
                  << (label.empty() ? "" : " (" + label + ")") << " ===\n";
        std::cout << "Batch size: " << metrics.batch_size << " items\n";
        std::cout << "Creation time: " << metrics.creation_time.count() << " μs\n";
        std::cout << "Processing time: " << metrics.processing_time.count() << " μs\n";
        std::cout << "Cleanup time: " << metrics.cleanup_time.count() << " μs\n";
        std::cout << "Memory used: " << metrics.memory_used << " bytes\n";
        std::cout << "Peak memory: " << metrics.peak_memory << " bytes\n";
        std::cout << "Items per second: " << static_cast<size_t>(metrics.items_per_second) << "\n";
        std::cout << "Memory efficiency: " << (metrics.memory_efficiency * 100.0) << "%\n";
        
        if (metrics.batch_size > 0) {
            std::cout << "Time per item (creation): " 
                      << (static_cast<double>(metrics.creation_time.count()) / metrics.batch_size) << " μs\n";
            std::cout << "Time per item (processing): " 
                      << (static_cast<double>(metrics.processing_time.count()) / metrics.batch_size) << " μs\n";
            std::cout << "Memory per item: " 
                      << (metrics.memory_used / metrics.batch_size) << " bytes\n";
        }
        
        std::cout << "================================================\n\n";
    }
};

#endif // MEMORY_PROFILER_H