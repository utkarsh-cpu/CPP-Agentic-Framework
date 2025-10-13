#include "../BaseNode/OptimizedBatchProcessor.h"
#include "../BaseNode/TypedData.h"
#include "../BaseNode/BatchData.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>

// Example processors for demonstration
namespace ExampleProcessors {
    // Simple mathematical operations
    auto square_processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
        if (auto float_data = data->as<TypedData<float>>()) {
            float value = float_data->get();
            return std::make_shared<TypedData<float>>(value * value);
        }
        return nullptr;
    };
    
    auto increment_processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
        if (auto int_data = data->as<TypedData<int>>()) {
            int value = int_data->get();
            return std::make_shared<TypedData<int>>(value + 1);
        }
        return nullptr;
    };
    
    // More complex processor that simulates some work
    auto complex_processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
        if (auto float_data = data->as<TypedData<float>>()) {
            float value = float_data->get();
            
            // Simulate some computational work
            float result = value;
            for (int i = 0; i < 100; ++i) {
                result = std::sin(result) * std::cos(result) + value;
            }
            
            return std::make_shared<TypedData<float>>(result);
        }
        return nullptr;
    };
}

// Utility functions for creating test data
std::unique_ptr<BatchData> create_float_batch(size_t size, float min_val = 0.0f, float max_val = 100.0f) {
    auto batch = std::make_unique<BatchData>(size);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min_val, max_val);
    
    for (size_t i = 0; i < size; ++i) {
        batch->add_typed_item(dis(gen));
    }
    
    return batch;
}

std::unique_ptr<BatchData> create_int_batch(size_t size, int min_val = 0, int max_val = 1000) {
    auto batch = std::make_unique<BatchData>(size);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(min_val, max_val);
    
    for (size_t i = 0; i < size; ++i) {
        batch->add_typed_item(dis(gen));
    }
    
    return batch;
}

// Performance measurement utility
class BenchmarkTimer {
private:
    std::chrono::steady_clock::time_point start_time_;
    std::string name_;
    
public:
    explicit BenchmarkTimer(std::string name) 
        : start_time_(std::chrono::steady_clock::now())
        , name_(std::move(name)) {}
    
    ~BenchmarkTimer() {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time_
        );
        
        std::cout << "[BENCHMARK] " << name_ << " completed in " 
                  << duration.count() << " ms" << std::endl;
    }
};

// Example functions demonstrating different use cases
void demonstrate_basic_processing() {
    std::cout << "\n=== Basic Batch Processing Demo ===" << std::endl;
    
    // Create processor with default configuration
    auto processor = create_optimized_processor(4, true);
    
    // Create test data
    auto input_batch = create_float_batch(1000);
    std::cout << "Created batch with " << input_batch->size() << " float items" << std::endl;
    
    // Process the batch
    {
        BenchmarkTimer timer("Basic float processing");
        auto result = processor->process_batch(*input_batch, ExampleProcessors::square_processor);
        
        if (result.has_value()) {
            auto& output_batch = result.value();
            std::cout << "Successfully processed " << output_batch->size() << " items" << std::endl;
            
            // Show some sample results
            if (output_batch->size() >= 5) {
                std::cout << "Sample results: ";
                for (size_t i = 0; i < 5; ++i) {
                    if (auto float_data = output_batch->at(i)->as<TypedData<float>>()) {
                        std::cout << std::fixed << std::setprecision(2) 
                                 << float_data->get() << " ";
                    }
                }
                std::cout << std::endl;
            }
        } else {
            std::cout << "Processing failed: " << result.error().what() << std::endl;
        }
    }
    
    // Print processor statistics
    std::cout << "Processor stats:" << std::endl;
    std::cout << "  - Batches processed: " << processor->batches_processed() << std::endl;
    std::cout << "  - Items processed: " << processor->items_processed() << std::endl;
    std::cout << "  - Average processing time: " << std::fixed << std::setprecision(2) 
              << processor->average_processing_time_ms() << " ms" << std::endl;
    std::cout << "  - Work stealing events: " << processor->work_stealing_count() << std::endl;
    std::cout << "  - Work item pool efficiency: " << processor->work_item_pool_efficiency() << "%" << std::endl;
}

void demonstrate_performance_comparison() {
    std::cout << "\n=== Performance Comparison Demo ===" << std::endl;
    
    const size_t batch_size = 10000;
    auto input_batch = create_float_batch(batch_size);
    
    // Test with different thread counts
    std::vector<size_t> thread_counts = {1, 2, 4, 8};
    
    for (size_t thread_count : thread_counts) {
        auto processor = create_optimized_processor(thread_count, true);
        
        std::cout << "\nTesting with " << thread_count << " threads:" << std::endl;
        
        {
            BenchmarkTimer timer("Complex processing (" + std::to_string(thread_count) + " threads)");
            auto result = processor->process_batch(*input_batch, ExampleProcessors::complex_processor);
            
            if (result.has_value()) {
                std::cout << "  - Successfully processed " << result.value()->size() << " items" << std::endl;
                std::cout << "  - Work stealing events: " << processor->work_stealing_count() << std::endl;
            } else {
                std::cout << "  - Processing failed: " << result.error().what() << std::endl;
            }
        }
    }
}

void demonstrate_simd_processing() {
    std::cout << "\n=== SIMD Processing Demo ===" << std::endl;
    
    // Test SIMD availability
    if (!SIMDOps::is_simd_supported()) {
        std::cout << "SIMD not supported on this system, using fallback implementations" << std::endl;
    } else {
        std::cout << "SIMD supported and enabled" << std::endl;
    }
    
    auto processor = create_high_performance_processor();
    auto input_batch = create_float_batch(10000);
    
    // Test SIMD-optimized numeric processing
    {
        BenchmarkTimer timer("SIMD-optimized processing");
        auto result = processor->process_numeric_batch_simd<float>(
            *input_batch,
            [](float x) { return x * 2.0f + 1.0f; }
        );
        
        if (result.has_value()) {
            std::cout << "SIMD processing completed successfully" << std::endl;
            std::cout << "Processed " << result.value()->size() << " items" << std::endl;
        } else {
            std::cout << "SIMD processing failed: " << result.error().what() << std::endl;
        }
    }
}

void demonstrate_specialized_operations() {
    std::cout << "\n=== Specialized Operations Demo ===" << std::endl;
    
    auto processor = create_optimized_processor(4, true);
    auto float_batch = create_float_batch(1000, 1.0f, 10.0f);
    
    // Demonstrate aggregation
    {
        BenchmarkTimer timer("Parallel aggregation");
        auto sum_result = SpecializedProcessors::aggregate_numeric_batch<float>(
            *float_batch,
            [](float a, float b) { return a + b; },
            0.0f,
            *processor
        );
        
        if (sum_result.has_value()) {
            std::cout << "Sum of all values: " << std::fixed << std::setprecision(2) 
                     << sum_result.value() << std::endl;
        }
    }
    
    // Demonstrate parallel map
    {
        BenchmarkTimer timer("Parallel map");
        auto map_result = SpecializedProcessors::parallel_map<float, float>(
            *float_batch,
            [](float x) { return std::sqrt(x); },
            *processor
        );
        
        if (map_result.has_value()) {
            std::cout << "Parallel map completed, result size: " 
                     << map_result.value()->size() << std::endl;
        }
    }
    
    // Demonstrate parallel filter
    {
        BenchmarkTimer timer("Parallel filter");
        auto filter_result = SpecializedProcessors::parallel_filter<float>(
            *float_batch,
            [](float x) { return x > 5.0f; },
            *processor
        );
        
        if (filter_result.has_value()) {
            std::cout << "Parallel filter completed, filtered size: " 
                     << filter_result.value()->size() << " (from " 
                     << float_batch->size() << ")" << std::endl;
        }
    }
}

void demonstrate_error_handling() {
    std::cout << "\n=== Error Handling Demo ===" << std::endl;
    
    auto processor = create_optimized_processor(2, false);
    
    // Test with empty batch
    {
        BatchData empty_batch;
        auto result = processor->process_batch(empty_batch, ExampleProcessors::square_processor);
        
        if (!result.has_value()) {
            std::cout << "Empty batch correctly rejected: " << result.error().what() << std::endl;
        }
    }
    
    // Test with batch containing null items
    {
        BatchData invalid_batch;
        invalid_batch.add_item(std::make_shared<TypedData<float>>(1.0f));
        try {
            invalid_batch.add_item(nullptr); // This should cause an error
        } catch (const std::exception& e) {
            std::cout << "Null item correctly rejected during batch creation: " << e.what() << std::endl;
        }
        
        // Only process if we have a valid batch (which we won't due to the exception above)
        if (!invalid_batch.empty()) {
            auto result = processor->process_batch(invalid_batch, ExampleProcessors::square_processor);
            
            if (!result.has_value()) {
                std::cout << "Invalid batch correctly rejected: " << result.error().what() << std::endl;
            }
        }
    }
    
    // Test with processor that throws exceptions
    auto throwing_processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
        if (auto float_data = data->as<TypedData<float>>()) {
            float value = float_data->get();
            if (value > 50.0f) {
                throw std::runtime_error("Value too large: " + std::to_string(value));
            }
            return std::make_shared<TypedData<float>>(value * 2.0f);
        }
        return nullptr;
    };
    
    {
        auto test_batch = create_float_batch(100, 0.0f, 100.0f); // Some values will be > 50
        auto result = processor->process_batch(*test_batch, throwing_processor);
        
        if (!result.has_value()) {
            std::cout << "Exception in processor correctly handled: " << result.error().what() << std::endl;
        }
    }
}

int main() {
    std::cout << "OptimizedBatchProcessor Example and Benchmark" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    std::cout << "System info:" << std::endl;
    std::cout << "  - Hardware threads: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << "  - SIMD support: " << (SIMDOps::is_simd_supported() ? "Yes" : "No") << std::endl;
    
    try {
        demonstrate_basic_processing();
        demonstrate_performance_comparison();
        demonstrate_simd_processing();
        demonstrate_specialized_operations();
        demonstrate_error_handling();
        
        std::cout << "\n=== All demonstrations completed successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}