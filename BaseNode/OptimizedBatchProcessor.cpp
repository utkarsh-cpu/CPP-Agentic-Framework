#include "OptimizedBatchProcessor.h"
#include "TypedData.h"
#include <iostream>
#include <iomanip>

// Implementation of template specializations and utility functions

namespace SIMDOps {
    // Check if SIMD is supported on this architecture
    bool is_simd_supported() {
        static bool checked = false;
        static bool supported = false;
        
        if (!checked) {
            #if defined(SIMD_X86_AVAILABLE) && defined(__AVX2__)
                supported = true;
            #elif defined(SIMD_ARM_AVAILABLE)
                supported = true;
            #else
                supported = false;
            #endif
            checked = true;
        }
        
        return supported;
    }
    
    // Architecture-specific SIMD implementations
    #if defined(SIMD_X86_AVAILABLE)
    // x86/x64 AVX2 implementations
    void add_arrays_simd(const float* a, const float* b, float* result, size_t size) {
        #ifdef __AVX2__
        size_t simd_size = size - (size % 8); // Process 8 floats at a time
        
        for (size_t i = 0; i < simd_size; i += 8) {
            __m256 va = _mm256_loadu_ps(&a[i]); // Use unaligned load for safety
            __m256 vb = _mm256_loadu_ps(&b[i]);
            __m256 vresult = _mm256_add_ps(va, vb);
            _mm256_storeu_ps(&result[i], vresult); // Use unaligned store for safety
        }
        
        // Handle remaining elements
        for (size_t i = simd_size; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
        #else
        add_arrays_fallback(a, b, result, size);
        #endif
    }
    
    void multiply_arrays_simd(const float* a, const float* b, float* result, size_t size) {
        #ifdef __AVX2__
        size_t simd_size = size - (size % 8);
        
        for (size_t i = 0; i < simd_size; i += 8) {
            __m256 va = _mm256_loadu_ps(&a[i]);
            __m256 vb = _mm256_loadu_ps(&b[i]);
            __m256 vresult = _mm256_mul_ps(va, vb);
            _mm256_storeu_ps(&result[i], vresult);
        }
        
        for (size_t i = simd_size; i < size; ++i) {
            result[i] = a[i] * b[i];
        }
        #else
        multiply_arrays_fallback(a, b, result, size);
        #endif
    }
    
    float sum_array_simd(const float* array, size_t size) {
        #ifdef __AVX2__
        __m256 sum_vec = _mm256_setzero_ps();
        size_t simd_size = size - (size % 8);
        
        for (size_t i = 0; i < simd_size; i += 8) {
            __m256 va = _mm256_loadu_ps(&array[i]);
            sum_vec = _mm256_add_ps(sum_vec, va);
        }
        
        // Extract and sum the 8 floats in the vector
        alignas(32) float temp[8];
        _mm256_storeu_ps(temp, sum_vec);
        float result = temp[0] + temp[1] + temp[2] + temp[3] + 
                      temp[4] + temp[5] + temp[6] + temp[7];
        
        // Add remaining elements
        for (size_t i = simd_size; i < size; ++i) {
            result += array[i];
        }
        
        return result;
        #else
        return sum_array_fallback(array, size);
        #endif
    }
    
    #elif defined(SIMD_ARM_AVAILABLE)
    // ARM NEON implementations
    void add_arrays_simd(const float* a, const float* b, float* result, size_t size) {
        size_t simd_size = size - (size % 4); // Process 4 floats at a time with NEON
        
        for (size_t i = 0; i < simd_size; i += 4) {
            float32x4_t va = vld1q_f32(&a[i]);
            float32x4_t vb = vld1q_f32(&b[i]);
            float32x4_t vresult = vaddq_f32(va, vb);
            vst1q_f32(&result[i], vresult);
        }
        
        // Handle remaining elements
        for (size_t i = simd_size; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }
    
    void multiply_arrays_simd(const float* a, const float* b, float* result, size_t size) {
        size_t simd_size = size - (size % 4);
        
        for (size_t i = 0; i < simd_size; i += 4) {
            float32x4_t va = vld1q_f32(&a[i]);
            float32x4_t vb = vld1q_f32(&b[i]);
            float32x4_t vresult = vmulq_f32(va, vb);
            vst1q_f32(&result[i], vresult);
        }
        
        for (size_t i = simd_size; i < size; ++i) {
            result[i] = a[i] * b[i];
        }
    }
    
    float sum_array_simd(const float* array, size_t size) {
        float32x4_t sum_vec = vdupq_n_f32(0.0f);
        size_t simd_size = size - (size % 4);
        
        for (size_t i = 0; i < simd_size; i += 4) {
            float32x4_t va = vld1q_f32(&array[i]);
            sum_vec = vaddq_f32(sum_vec, va);
        }
        
        // Extract and sum the 4 floats in the vector
        float temp[4];
        vst1q_f32(temp, sum_vec);
        float result = temp[0] + temp[1] + temp[2] + temp[3];
        
        // Add remaining elements
        for (size_t i = simd_size; i < size; ++i) {
            result += array[i];
        }
        
        return result;
    }
    
    #else
    // Fallback implementations for unsupported architectures
    void add_arrays_simd(const float* a, const float* b, float* result, size_t size) {
        add_arrays_fallback(a, b, result, size);
    }
    
    void multiply_arrays_simd(const float* a, const float* b, float* result, size_t size) {
        multiply_arrays_fallback(a, b, result, size);
    }
    
    float sum_array_simd(const float* array, size_t size) {
        return sum_array_fallback(array, size);
    }
    #endif
}

// Utility functions for OptimizedBatchProcessor

namespace BatchProcessorUtils {
    // Calculate optimal chunk size for parallel processing
    size_t calculate_optimal_chunk_size(size_t total_items, size_t thread_count) {
        if (total_items == 0 || thread_count == 0) {
            return 1;
        }
        
        // Aim for at least 2 chunks per thread to enable work stealing
        size_t min_chunks = thread_count * 2;
        size_t chunk_size = total_items / min_chunks;
        
        // Ensure minimum chunk size for efficiency
        const size_t min_chunk_size = 10;
        const size_t max_chunk_size = 1000;
        
        chunk_size = std::max(chunk_size, min_chunk_size);
        chunk_size = std::min(chunk_size, max_chunk_size);
        
        return chunk_size;
    }
    
    // Validate batch data for processing
    bool validate_batch_data(const BatchData& batch) {
        if (batch.empty()) {
            return false;
        }
        
        // Check for null items
        for (const auto& item : batch) {
            if (!item) {
                return false;
            }
        }
        
        return true;
    }
    
    // Performance monitoring helper
    class PerformanceMonitor {
    private:
        std::chrono::steady_clock::time_point start_time_;
        std::string operation_name_;
        
    public:
        explicit PerformanceMonitor(std::string name) 
            : start_time_(std::chrono::steady_clock::now())
            , operation_name_(std::move(name)) {}
        
        ~PerformanceMonitor() {
            #ifdef DEBUG_PERFORMANCE
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - start_time_
            );
            std::cout << "[PERF] " << operation_name_ << " took " 
                     << duration.count() << " microseconds" << std::endl;
            #else
            // Suppress unused variable warning when DEBUG_PERFORMANCE is not defined
            (void)start_time_;
            (void)operation_name_;
            #endif
        }
        
        [[nodiscard]] std::chrono::microseconds elapsed() const {
            auto current_time = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::microseconds>(
                current_time - start_time_
            );
        }
    };
}

// Example usage and factory functions

std::unique_ptr<OptimizedBatchProcessor> create_optimized_processor(
    size_t thread_count,
    bool enable_simd) {
    
    OptimizedBatchProcessor::Config config;
    config.thread_count = thread_count > 0 ? thread_count : std::thread::hardware_concurrency();
    config.enable_simd = enable_simd && SIMDOps::is_simd_supported();
    
    return std::make_unique<OptimizedBatchProcessor>(config);
}

std::unique_ptr<OptimizedBatchProcessor> create_high_performance_processor() {
    OptimizedBatchProcessor::Config config;
    config.thread_count = std::thread::hardware_concurrency();
    config.min_parallel_threshold = 50;
    config.enable_simd = SIMDOps::is_simd_supported();
    config.work_item_pool_size = 2000;
    
    return std::make_unique<OptimizedBatchProcessor>(config);
}

std::unique_ptr<OptimizedBatchProcessor> create_memory_efficient_processor() {
    OptimizedBatchProcessor::Config config;
    config.thread_count = std::max(2u, std::thread::hardware_concurrency() / 2);
    config.min_parallel_threshold = 200;
    config.enable_simd = false; // Disable SIMD to save memory
    config.work_item_pool_size = 500;
    
    return std::make_unique<OptimizedBatchProcessor>(config);
}

// Specialized batch processing functions are now implemented in the header file

// Template instantiations are no longer needed since templates are in the header