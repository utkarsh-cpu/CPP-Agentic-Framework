#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include "../BaseNode/OptimizedBatchProcessor.h"
#include "../BaseNode/BatchData.h"
#include "../BaseNode/TypedData.h"
#include "MemoryProfiler.h"
#include <memory>
#include <vector>
#include <random>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <future>
#include <map>
#include <iostream>
#include <iomanip>
#include <cmath>

// Test fixture for concurrency and performance benchmarks
class ConcurrencyPerformanceBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data
        setupTestData();
        
        // Create processors with different configurations
        setupProcessors();
    }
    
    void TearDown() override {
        // Cleanup
        processors_.clear();
    }
    
    void setupTestData() {
        std::mt19937 rng(42); // Fixed seed for reproducible results
        
        // Create test data pools of different sizes
        small_int_pool_.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            small_int_pool_.push_back(std::make_shared<TypedData<int>>(rng()));
        }
        
        medium_float_pool_.reserve(100000);
        for (int i = 0; i < 100000; ++i) {
            medium_float_pool_.push_back(
                std::make_shared<TypedData<float>>(static_cast<float>(rng()) / rng.max())
            );
        }
        
        large_double_pool_.reserve(1000000);
        for (int i = 0; i < 1000000; ++i) {
            large_double_pool_.push_back(
                std::make_shared<TypedData<double>>(static_cast<double>(i) * 0.001)
            );
        }
    }
    
    void setupProcessors() {
        // Single-threaded processor
        OptimizedBatchProcessor::Config single_config;
        single_config.thread_count = 1;
        single_config.enable_simd = false;
        processors_["single"] = std::make_unique<OptimizedBatchProcessor>(single_config);
        
        // Multi-threaded processor (default)
        processors_["multi"] = std::make_unique<OptimizedBatchProcessor>();
        
        // High-performance processor
        processors_["high_perf"] = create_high_performance_processor();
        
        // Memory-efficient processor
        processors_["memory_eff"] = create_memory_efficient_processor();
        
        // SIMD-enabled processor
        OptimizedBatchProcessor::Config simd_config;
        simd_config.enable_simd = true;
        simd_config.thread_count = std::thread::hardware_concurrency();
        processors_["simd"] = std::make_unique<OptimizedBatchProcessor>(simd_config);
    }
    
protected:
    std::vector<std::shared_ptr<TypedData<int>>> small_int_pool_;
    std::vector<std::shared_ptr<TypedData<float>>> medium_float_pool_;
    std::vector<std::shared_ptr<TypedData<double>>> large_double_pool_;
    
    std::map<std::string, std::unique_ptr<OptimizedBatchProcessor>> processors_;
};

// Test 1: Thread Pool Efficiency Under Various Load Conditions
TEST_F(ConcurrencyPerformanceBenchmarkTest, ThreadPoolEfficiencyTest) {
    std::cout << "\n=== Thread Pool Efficiency Test ===\n";
    
    // Test different load conditions
    std::vector<size_t> load_sizes = {100, 1000, 10000, 100000};
    std::vector<size_t> thread_counts = {1, 2, 4, 8, std::thread::hardware_concurrency()};
    
    for (size_t load_size : load_sizes) {
        if (load_size > medium_float_pool_.size()) continue;
        
        std::cout << "\nLoad size: " << load_size << " items\n";
        std::cout << "Threads\tTime(ms)\tThroughput(items/s)\tEfficiency\tWork Stolen\n";
        
        // Create batch for this load size
        BatchData test_batch;
        test_batch.reserve(load_size);
        for (size_t i = 0; i < load_size; ++i) {
            test_batch.add_item(medium_float_pool_[i]);
        }
        
        // Simple processing function (square each value)
        auto processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
            if (auto typed_data = data->template as<TypedData<float>>()) {
                float value = typed_data->get();
                return std::make_shared<TypedData<float>>(value * value);
            }
            return nullptr;
        };
        
        double baseline_time = 0.0;
        
        for (size_t thread_count : thread_counts) {
            // Create processor with specific thread count
            OptimizedBatchProcessor::Config config;
            config.thread_count = thread_count;
            config.enable_simd = false; // Disable SIMD for pure thread pool testing
            auto test_processor = std::make_unique<OptimizedBatchProcessor>(config);
            
            // Measure processing time
            auto start_time = std::chrono::high_resolution_clock::now();
            auto result = test_processor->process_batch(test_batch, processor);
            auto end_time = std::chrono::high_resolution_clock::now();
            
            ASSERT_TRUE(result.has_value()) << "Batch processing failed";
            EXPECT_EQ(result.value()->size(), load_size);
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            double time_ms = duration.count();
            
            if (thread_count == 1) {
                baseline_time = time_ms;
            }
            
            double throughput = (load_size * 1000.0) / time_ms; // items per second
            double efficiency = baseline_time / (time_ms * thread_count); // Ideal efficiency = 1.0
            size_t work_stolen = test_processor->work_stealing_count();
            
            std::cout << thread_count << "\t" 
                      << std::fixed << std::setprecision(2) << time_ms << "\t\t"
                      << std::fixed << std::setprecision(0) << throughput << "\t\t"
                      << std::fixed << std::setprecision(3) << efficiency << "\t\t"
                      << work_stolen << "\n";
            
            // Verify reasonable efficiency for multi-threading
            if (thread_count > 1 && load_size >= 1000) {
                EXPECT_GT(efficiency, 0.3) << "Thread efficiency too low for " << thread_count << " threads";
                EXPECT_LT(time_ms, baseline_time * 1.2) << "Multi-threading should improve or maintain performance";
            }
        }
    }
}

// Test 2: Work Stealing and Load Balancing Behavior
TEST_F(ConcurrencyPerformanceBenchmarkTest, WorkStealingLoadBalancingTest) {
    std::cout << "\n=== Work Stealing and Load Balancing Test ===\n";
    
    const size_t num_threads = std::thread::hardware_concurrency();
    const size_t total_items = 50000;
    
    // Create uneven workload - some items take longer to process
    BatchData uneven_batch;
    uneven_batch.reserve(total_items);
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> work_dist(1, 100);
    
    for (size_t i = 0; i < total_items; ++i) {
        int work_amount = work_dist(rng);
        uneven_batch.add_typed_item(work_amount);
    }
    
    // Processor that simulates variable work (busy wait)
    auto variable_processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
        if (auto typed_data = data->template as<TypedData<int>>()) {
            int work_amount = typed_data->get();
            
            // Simulate work with busy wait
            auto start = std::chrono::high_resolution_clock::now();
            volatile int dummy = 0;
            while (std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - start).count() < work_amount) {
                dummy++;
            }
            
            return std::make_shared<TypedData<int>>(work_amount * 2);
        }
        return nullptr;
    };
    
    // Test with different thread counts
    std::vector<size_t> thread_counts = {1, 2, 4, num_threads};
    
    std::cout << "Threads\tTime(ms)\tWork Stolen\tSteal Rate(%)\tLoad Balance Score\n";
    
    for (size_t thread_count : thread_counts) {
        OptimizedBatchProcessor::Config config;
        config.thread_count = thread_count;
        auto test_processor = std::make_unique<OptimizedBatchProcessor>(config);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        auto result = test_processor->process_batch(uneven_batch, variable_processor);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        ASSERT_TRUE(result.has_value()) << "Batch processing failed";
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        size_t work_stolen = test_processor->work_stealing_count();
        size_t tasks_completed = test_processor->thread_count() > 0 ? total_items : 0;
        
        double steal_rate = tasks_completed > 0 ? (work_stolen * 100.0) / tasks_completed : 0.0;
        
        // Calculate load balance score (simplified - in practice would measure per-thread work)
        double load_balance_score = thread_count > 1 ? std::min(1.0, steal_rate / 10.0) : 1.0;
        
        std::cout << thread_count << "\t"
                  << duration.count() << "\t\t"
                  << work_stolen << "\t\t"
                  << std::fixed << std::setprecision(1) << steal_rate << "\t\t"
                  << std::fixed << std::setprecision(3) << load_balance_score << "\n";
        
        // Verify work stealing is happening with multiple threads
        if (thread_count > 1) {
            EXPECT_GT(work_stolen, 0) << "Work stealing should occur with uneven workload";
            EXPECT_GT(steal_rate, 1.0) << "Steal rate should be reasonable for load balancing";
        }
    }
    
    // Test work stealing efficiency with extreme imbalance
    std::cout << "\n--- Extreme Imbalance Test ---\n";
    
    BatchData extreme_batch;
    extreme_batch.reserve(1000);
    
    // First 10 items are very expensive, rest are cheap
    for (int i = 0; i < 10; ++i) {
        extreme_batch.add_typed_item(10000); // 10ms work
    }
    for (int i = 0; i < 990; ++i) {
        extreme_batch.add_typed_item(10); // 10μs work
    }
    
    OptimizedBatchProcessor::Config extreme_config;
    extreme_config.thread_count = num_threads;
    auto extreme_processor = std::make_unique<OptimizedBatchProcessor>(extreme_config);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto result = extreme_processor->process_batch(extreme_batch, variable_processor);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    ASSERT_TRUE(result.has_value());
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    size_t work_stolen = extreme_processor->work_stealing_count();
    
    std::cout << "Extreme imbalance - Time: " << duration.count() << "ms, Work stolen: " << work_stolen << "\n";
    
    // With extreme imbalance, work stealing should be very active
    EXPECT_GT(work_stolen, 50) << "Work stealing should be very active with extreme imbalance";
}

// Test 3: SIMD Operations vs Scalar Implementations
TEST_F(ConcurrencyPerformanceBenchmarkTest, SIMDvScalarBenchmark) {
    std::cout << "\n=== SIMD vs Scalar Benchmark ===\n";
    
    // Test different array sizes for SIMD effectiveness
    std::vector<size_t> array_sizes = {100, 1000, 10000, 100000, 1000000};
    
    std::cout << "Array Size\tScalar(μs)\tSIMD(μs)\tSpeedup\tSIMD Available\n";
    
    for (size_t size : array_sizes) {
        if (size > medium_float_pool_.size()) continue;
        
        // Prepare test arrays
        std::vector<float> array_a(size);
        std::vector<float> array_b(size);
        std::vector<float> result_scalar(size);
        std::vector<float> result_simd(size);
        
        // Fill with test data
        for (size_t i = 0; i < size; ++i) {
            if (i < medium_float_pool_.size()) {
                array_a[i] = medium_float_pool_[i]->get();
                array_b[i] = medium_float_pool_[i]->get() * 0.5f;
            }
        }
        
        // Benchmark scalar addition
        auto scalar_start = std::chrono::high_resolution_clock::now();
        SIMDOps::add_arrays_fallback(array_a.data(), array_b.data(), result_scalar.data(), size);
        auto scalar_end = std::chrono::high_resolution_clock::now();
        auto scalar_time = std::chrono::duration_cast<std::chrono::microseconds>(scalar_end - scalar_start);
        
        // Benchmark SIMD addition
        auto simd_start = std::chrono::high_resolution_clock::now();
        SIMDOps::add_arrays_simd(array_a.data(), array_b.data(), result_simd.data(), size);
        auto simd_end = std::chrono::high_resolution_clock::now();
        auto simd_time = std::chrono::duration_cast<std::chrono::microseconds>(simd_end - simd_start);
        
        // Verify results are equivalent (within floating point tolerance)
        bool results_match = true;
        for (size_t i = 0; i < size; ++i) {
            if (std::abs(result_scalar[i] - result_simd[i]) > 1e-6f) {
                results_match = false;
                break;
            }
        }
        
        EXPECT_TRUE(results_match) << "SIMD and scalar results should match for size " << size;
        
        double speedup = scalar_time.count() > 0 ? 
            static_cast<double>(scalar_time.count()) / simd_time.count() : 1.0;
        
        std::cout << size << "\t\t"
                  << scalar_time.count() << "\t\t"
                  << simd_time.count() << "\t\t"
                  << std::fixed << std::setprecision(2) << speedup << "x\t\t"
                  << (SIMDOps::is_simd_supported() ? "Yes" : "No") << "\n";
        
        // SIMD should provide speedup for larger arrays (if supported)
        if (SIMDOps::is_simd_supported() && size >= 1000) {
            EXPECT_GE(speedup, 1.0) << "SIMD should not be slower than scalar for size " << size;
            if (size >= 10000) {
                EXPECT_GE(speedup, 1.5) << "SIMD should provide significant speedup for large arrays";
            }
        }
    }
    
    // Test different SIMD operations
    std::cout << "\n--- SIMD Operations Comparison ---\n";
    const size_t test_size = 100000;
    
    if (test_size <= medium_float_pool_.size()) {
        std::vector<float> test_array(test_size);
        std::vector<float> array_a(test_size);
        std::vector<float> array_b(test_size);
        std::vector<float> result(test_size);
        
        // Fill test arrays
        for (size_t i = 0; i < test_size; ++i) {
            test_array[i] = medium_float_pool_[i]->get();
            array_a[i] = medium_float_pool_[i]->get();
            array_b[i] = medium_float_pool_[i]->get() * 0.5f;
        }
        
        std::cout << "Operation\tScalar(μs)\tSIMD(μs)\tSpeedup\n";
        
        // Test addition
        {
            auto scalar_start = std::chrono::high_resolution_clock::now();
            SIMDOps::add_arrays_fallback(array_a.data(), array_b.data(), result.data(), test_size);
            auto scalar_end = std::chrono::high_resolution_clock::now();
            auto scalar_time = std::chrono::duration_cast<std::chrono::microseconds>(scalar_end - scalar_start);
            
            auto simd_start = std::chrono::high_resolution_clock::now();
            SIMDOps::add_arrays_simd(array_a.data(), array_b.data(), result.data(), test_size);
            auto simd_end = std::chrono::high_resolution_clock::now();
            auto simd_time = std::chrono::duration_cast<std::chrono::microseconds>(simd_end - simd_start);
            
            double speedup = scalar_time.count() > 0 ? 
                static_cast<double>(scalar_time.count()) / simd_time.count() : 1.0;
            
            std::cout << "Addition\t" << scalar_time.count() << "\t\t" 
                      << simd_time.count() << "\t\t" 
                      << std::fixed << std::setprecision(2) << speedup << "x\n";
        }
        
        // Test multiplication
        {
            auto scalar_start = std::chrono::high_resolution_clock::now();
            SIMDOps::multiply_arrays_fallback(array_a.data(), array_b.data(), result.data(), test_size);
            auto scalar_end = std::chrono::high_resolution_clock::now();
            auto scalar_time = std::chrono::duration_cast<std::chrono::microseconds>(scalar_end - scalar_start);
            
            auto simd_start = std::chrono::high_resolution_clock::now();
            SIMDOps::multiply_arrays_simd(array_a.data(), array_b.data(), result.data(), test_size);
            auto simd_end = std::chrono::high_resolution_clock::now();
            auto simd_time = std::chrono::duration_cast<std::chrono::microseconds>(simd_end - simd_start);
            
            double speedup = scalar_time.count() > 0 ? 
                static_cast<double>(scalar_time.count()) / simd_time.count() : 1.0;
            
            std::cout << "Multiply\t" << scalar_time.count() << "\t\t" 
                      << simd_time.count() << "\t\t" 
                      << std::fixed << std::setprecision(2) << speedup << "x\n";
        }
        
        // Test sum reduction
        {
            auto scalar_start = std::chrono::high_resolution_clock::now();
            float scalar_sum = SIMDOps::sum_array_fallback(test_array.data(), test_size);
            auto scalar_end = std::chrono::high_resolution_clock::now();
            auto scalar_time = std::chrono::duration_cast<std::chrono::microseconds>(scalar_end - scalar_start);
            
            auto simd_start = std::chrono::high_resolution_clock::now();
            float simd_sum = SIMDOps::sum_array_simd(test_array.data(), test_size);
            auto simd_end = std::chrono::high_resolution_clock::now();
            auto simd_time = std::chrono::duration_cast<std::chrono::microseconds>(simd_end - simd_start);
            
            // Verify results match
            EXPECT_NEAR(scalar_sum, simd_sum, 1e-3f) << "Sum results should match";
            
            double speedup = scalar_time.count() > 0 ? 
                static_cast<double>(scalar_time.count()) / simd_time.count() : 1.0;
            
            std::cout << "Sum\t\t" << scalar_time.count() << "\t\t" 
                      << simd_time.count() << "\t\t" 
                      << std::fixed << std::setprecision(2) << speedup << "x\n";
        }
    }
}

// Test 4: Integrated Performance Test (Thread Pool + SIMD)
TEST_F(ConcurrencyPerformanceBenchmarkTest, IntegratedPerformanceTest) {
    std::cout << "\n=== Integrated Performance Test (Thread Pool + SIMD) ===\n";
    
    const size_t batch_size = 100000;
    if (batch_size > medium_float_pool_.size()) {
        GTEST_SKIP() << "Not enough test data for integrated performance test";
    }
    
    // Create test batch
    BatchData test_batch;
    test_batch.reserve(batch_size);
    for (size_t i = 0; i < batch_size; ++i) {
        test_batch.add_item(medium_float_pool_[i]);
    }
    
    // Simple numeric processor (square each value)
    auto numeric_processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
        if (auto typed_data = data->template as<TypedData<float>>()) {
            float value = typed_data->get();
            return std::make_shared<TypedData<float>>(value * value);
        }
        return nullptr;
    };
    
    std::cout << "Configuration\t\tTime(ms)\tThroughput(items/s)\tMemory Efficiency(%)\n";
    
    // Test different processor configurations
    std::vector<std::pair<std::string, std::string>> configs = {
        {"single", "Single Thread, No SIMD"},
        {"multi", "Multi Thread, No SIMD"},
        {"simd", "Multi Thread, SIMD"},
        {"high_perf", "High Performance"},
        {"memory_eff", "Memory Efficient"}
    };
    
    for (const auto& [config_name, description] : configs) {
        if (processors_.find(config_name) == processors_.end()) continue;
        
        auto& processor = processors_[config_name];
        
        // Measure performance with memory profiling
        MemoryProfiler::enable();
        MemoryProfiler::reset();
        
        auto start_time = std::chrono::high_resolution_clock::now();
        auto result = processor->process_batch(test_batch, numeric_processor);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto memory_stats = MemoryProfiler::get_stats();
        MemoryProfiler::disable();
        
        ASSERT_TRUE(result.has_value()) << "Processing failed for " << config_name;
        EXPECT_EQ(result.value()->size(), batch_size);
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        double throughput = (batch_size * 1000.0) / duration.count();
        double memory_efficiency = processor->work_item_pool_efficiency();
        
        std::cout << std::left << std::setw(20) << description << "\t"
                  << duration.count() << "\t\t"
                  << std::fixed << std::setprecision(0) << throughput << "\t\t\t"
                  << std::fixed << std::setprecision(1) << memory_efficiency << "\n";
        
        // Verify reasonable performance characteristics
        EXPECT_GT(throughput, 1000) << "Throughput should be reasonable for " << config_name;
        EXPECT_GT(memory_efficiency, 50) << "Memory efficiency should be reasonable for " << config_name;
    }
}

// Test 5: Scalability Analysis
TEST_F(ConcurrencyPerformanceBenchmarkTest, ScalabilityAnalysis) {
    std::cout << "\n=== Scalability Analysis ===\n";
    
    // Test how performance scales with batch size and thread count
    std::vector<size_t> batch_sizes = {1000, 10000, 100000};
    std::vector<size_t> thread_counts = {1, 2, 4, 8};
    
    auto simple_processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
        if (auto typed_data = data->template as<TypedData<float>>()) {
            float value = typed_data->get();
            // Simple computation to simulate work
            float result = std::sqrt(value * value + 1.0f);
            return std::make_shared<TypedData<float>>(result);
        }
        return nullptr;
    };
    
    std::cout << "Batch Size\tThreads\tTime(ms)\tThroughput\tScaling Efficiency\n";
    
    for (size_t batch_size : batch_sizes) {
        if (batch_size > medium_float_pool_.size()) continue;
        
        // Create test batch
        BatchData test_batch;
        test_batch.reserve(batch_size);
        for (size_t i = 0; i < batch_size; ++i) {
            test_batch.add_item(medium_float_pool_[i]);
        }
        
        double baseline_time = 0.0;
        
        for (size_t thread_count : thread_counts) {
            OptimizedBatchProcessor::Config config;
            config.thread_count = thread_count;
            config.enable_simd = false; // Focus on threading scalability
            auto test_processor = std::make_unique<OptimizedBatchProcessor>(config);
            
            // Run multiple iterations for stable measurements
            const int iterations = 3;
            std::vector<double> times;
            
            for (int i = 0; i < iterations; ++i) {
                auto start_time = std::chrono::high_resolution_clock::now();
                auto result = test_processor->process_batch(test_batch, simple_processor);
                auto end_time = std::chrono::high_resolution_clock::now();
                
                ASSERT_TRUE(result.has_value());
                
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                times.push_back(duration.count());
            }
            
            // Use median time for stability
            std::sort(times.begin(), times.end());
            double median_time = times[times.size() / 2];
            
            if (thread_count == 1) {
                baseline_time = median_time;
            }
            
            double throughput = (batch_size * 1000.0) / median_time;
            double scaling_efficiency = baseline_time / (median_time * thread_count);
            
            std::cout << batch_size << "\t\t" << thread_count << "\t"
                      << std::fixed << std::setprecision(1) << median_time << "\t\t"
                      << std::fixed << std::setprecision(0) << throughput << "\t\t"
                      << std::fixed << std::setprecision(3) << scaling_efficiency << "\n";
            
            // Verify scaling behavior
            if (thread_count > 1 && batch_size >= 10000) {
                EXPECT_GT(scaling_efficiency, 0.2) << "Scaling efficiency too low";
                EXPECT_LE(median_time, baseline_time * 1.5) << "Performance should not degrade significantly";
            }
        }
        
        std::cout << "\n";
    }
}

// Google Benchmark integration for detailed performance profiling
static void BM_ThreadPoolEfficiency(benchmark::State& state) {
    const size_t thread_count = state.range(0);
    const size_t batch_size = state.range(1);
    
    // Setup test data
    std::vector<std::shared_ptr<TypedData<float>>> test_data;
    test_data.reserve(batch_size);
    std::mt19937 rng(42);
    for (size_t i = 0; i < batch_size; ++i) {
        test_data.push_back(std::make_shared<TypedData<float>>(static_cast<float>(rng()) / rng.max()));
    }
    
    BatchData test_batch;
    test_batch.reserve(batch_size);
    for (const auto& item : test_data) {
        test_batch.add_item(item);
    }
    
    OptimizedBatchProcessor::Config config;
    config.thread_count = thread_count;
    auto processor = std::make_unique<OptimizedBatchProcessor>(config);
    
    auto simple_processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
        if (auto typed_data = data->template as<TypedData<float>>()) {
            float value = typed_data->get();
            return std::make_shared<TypedData<float>>(value * value);
        }
        return nullptr;
    };
    
    for (auto _ : state) {
        auto result = processor->process_batch(test_batch, simple_processor);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
    state.SetBytesProcessed(state.iterations() * batch_size * sizeof(float));
}

// Register benchmarks
BENCHMARK(BM_ThreadPoolEfficiency)
    ->Args({1, 1000})
    ->Args({2, 1000})
    ->Args({4, 1000})
    ->Args({8, 1000})
    ->Args({1, 10000})
    ->Args({2, 10000})
    ->Args({4, 10000})
    ->Args({8, 10000})
    ->Unit(benchmark::kMillisecond);

static void BM_SIMDOperations(benchmark::State& state) {
    const size_t array_size = state.range(0);
    const bool use_simd = state.range(1);
    
    std::vector<float> array_a(array_size);
    std::vector<float> array_b(array_size);
    std::vector<float> result(array_size);
    
    std::mt19937 rng(42);
    for (size_t i = 0; i < array_size; ++i) {
        array_a[i] = static_cast<float>(rng()) / rng.max();
        array_b[i] = static_cast<float>(rng()) / rng.max();
    }
    
    for (auto _ : state) {
        if (use_simd) {
            SIMDOps::add_arrays_simd(array_a.data(), array_b.data(), result.data(), array_size);
        } else {
            SIMDOps::add_arrays_fallback(array_a.data(), array_b.data(), result.data(), array_size);
        }
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * array_size);
    state.SetBytesProcessed(state.iterations() * array_size * sizeof(float) * 3); // 2 inputs + 1 output
}

BENCHMARK(BM_SIMDOperations)
    ->Args({1000, 0})    // Scalar
    ->Args({1000, 1})    // SIMD
    ->Args({10000, 0})   // Scalar
    ->Args({10000, 1})   // SIMD
    ->Args({100000, 0})  // Scalar
    ->Args({100000, 1})  // SIMD
    ->Unit(benchmark::kMicrosecond);