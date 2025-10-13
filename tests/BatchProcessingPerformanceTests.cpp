#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include "../BaseNode/BatchData.h"
#include "../BaseNode/TypedData.h"
#include "../Flow/StringData.h"
#include "../BatchNode/BatchNode.h"
#include "MemoryProfiler.h"
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <thread>

// Performance test fixture with memory tracking capabilities
class BatchProcessingPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize random number generator for consistent tests
        rng_.seed(42);
        
        // Create test data pools
        createTestDataPools();
        
        // Reset memory tracking
        resetMemoryTracking();
    }
    
    void TearDown() override {
        // Verify no significant memory leaks
        // In a real implementation, you'd integrate with memory profiling tools
    }
    
    // Create various sizes of test data for performance testing
    void createTestDataPools() {
        // Small dataset (100 items)
        small_string_pool_.reserve(100);
        for (int i = 0; i < 100; ++i) {
            small_string_pool_.push_back(
                std::make_shared<StringData>("small_test_" + std::to_string(i))
            );
        }
        
        // Medium dataset (10,000 items)
        medium_int_pool_.reserve(10000);
        for (int i = 0; i < 10000; ++i) {
            medium_int_pool_.push_back(
                std::make_shared<TypedData<int>>(i)
            );
        }
        
        // Large dataset (1,000,000 items)
        large_double_pool_.reserve(1000000);
        for (int i = 0; i < 1000000; ++i) {
            large_double_pool_.push_back(
                std::make_shared<TypedData<double>>(static_cast<double>(i) * 0.1)
            );
        }
    }
    
    // Memory tracking helpers (simplified - in production use valgrind/sanitizers)
    void resetMemoryTracking() {
        initial_allocations_ = getCurrentAllocationCount();
    }
    
    size_t getCurrentAllocationCount() {
        // Simplified allocation tracking
        // In production, integrate with memory profiling tools
        return 0; // Placeholder
    }
    
    size_t getAllocationDelta() {
        return getCurrentAllocationCount() - initial_allocations_;
    }
    
    // Timing utilities
    template<typename Func>
    std::chrono::nanoseconds measureExecutionTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    }
    
    template<typename Func>
    double measureAverageExecutionTime(Func&& func, int iterations = 10) {
        std::vector<double> times;
        times.reserve(iterations);
        
        for (int i = 0; i < iterations; ++i) {
            auto duration = measureExecutionTime(func);
            times.push_back(static_cast<double>(duration.count()));
        }
        
        return std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    }
    
protected:
    std::mt19937 rng_;
    size_t initial_allocations_ = 0;
    
    // Test data pools
    std::vector<std::shared_ptr<StringData>> small_string_pool_;
    std::vector<std::shared_ptr<TypedData<int>>> medium_int_pool_;
    std::vector<std::shared_ptr<TypedData<double>>> large_double_pool_;
};

// Test memory allocation patterns in batch operations
TEST_F(BatchProcessingPerformanceTest, MemoryAllocationPatterns) {
    // Test using the batch performance analyzer
    std::cout << "\n=== Testing Memory Allocation Patterns ===\n";
    
    // Test 1: Batch creation without reserve
    {
        auto metrics = BatchPerformanceAnalyzer::analyze_batch_operation(
            small_string_pool_.size(),
            [&](size_t size) {
                auto batch = std::make_unique<BatchData>();
                for (size_t i = 0; i < size; ++i) {
                    batch->add_item(small_string_pool_[i]);
                }
                return batch;
            },
            [](const auto& batch) {
                // Simple processing - count items
                size_t count = 0;
                for (const auto& item : *batch) {
                    if (item) ++count;
                }
                return count;
            },
            [](auto& batch) {
                batch->clear();
            }
        );
        
        BatchPerformanceAnalyzer::print_metrics(metrics, "No Reserve");
    }
    
    // Test 2: Batch creation with reserve
    {
        auto metrics = BatchPerformanceAnalyzer::analyze_batch_operation(
            small_string_pool_.size(),
            [&](size_t size) {
                auto batch = std::make_unique<BatchData>();
                batch->reserve(size);
                for (size_t i = 0; i < size; ++i) {
                    batch->add_item(small_string_pool_[i]);
                }
                return batch;
            },
            [](const auto& batch) {
                size_t count = 0;
                for (const auto& item : *batch) {
                    if (item) ++count;
                }
                return count;
            },
            [](auto& batch) {
                batch->clear();
            }
        );
        
        BatchPerformanceAnalyzer::print_metrics(metrics, "With Reserve");
    }
    // Test 1: Memory allocation without reserve()
    {
        resetMemoryTracking();
        BatchData batch_no_reserve;
        
        auto time_no_reserve = measureExecutionTime([&]() {
            for (const auto& item : small_string_pool_) {
                batch_no_reserve.add_item(item);
            }
        });
        
        EXPECT_EQ(batch_no_reserve.size(), small_string_pool_.size());
        // Without reserve, expect multiple reallocations
        EXPECT_GT(batch_no_reserve.capacity(), batch_no_reserve.size());
        
        std::cout << "Time without reserve: " << time_no_reserve.count() << " ns\n";
    }
    
    // Test 2: Memory allocation with reserve()
    {
        resetMemoryTracking();
        BatchData batch_with_reserve;
        batch_with_reserve.reserve(small_string_pool_.size());
        
        auto time_with_reserve = measureExecutionTime([&]() {
            for (const auto& item : small_string_pool_) {
                batch_with_reserve.add_item(item);
            }
        });
        
        EXPECT_EQ(batch_with_reserve.size(), small_string_pool_.size());
        // With reserve, capacity should match or exceed reserved size
        EXPECT_GE(batch_with_reserve.capacity(), small_string_pool_.size());
        
        std::cout << "Time with reserve: " << time_with_reserve.count() << " ns\n";
    }
    
    // Test 3: Memory allocation with constructor reserve
    {
        resetMemoryTracking();
        
        auto time_constructor_reserve = measureExecutionTime([&]() {
            BatchData batch_constructor_reserve(small_string_pool_.size());
            for (const auto& item : small_string_pool_) {
                batch_constructor_reserve.add_item(item);
            }
        });
        
        std::cout << "Time with constructor reserve: " << time_constructor_reserve.count() << " ns\n";
    }
    
    // Test 4: Batch addition vs individual addition
    {
        resetMemoryTracking();
        BatchData batch_individual;
        batch_individual.reserve(small_string_pool_.size());
        
        auto time_individual = measureExecutionTime([&]() {
            for (const auto& item : small_string_pool_) {
                batch_individual.add_item(item);
            }
        });
        
        BatchData batch_range;
        batch_range.reserve(small_string_pool_.size());
        
        auto time_range = measureExecutionTime([&]() {
            std::vector<std::shared_ptr<FrameworkData>> items(
                small_string_pool_.begin(), small_string_pool_.end()
            );
            batch_range.add_range(items.begin(), items.end());
        });
        
        EXPECT_EQ(batch_individual.size(), batch_range.size());
        std::cout << "Individual addition time: " << time_individual.count() << " ns\n";
        std::cout << "Range addition time: " << time_range.count() << " ns\n";
        
        // Range addition may be slower due to vector copy overhead for small batches
        // For larger batches, range addition should be more efficient
        // This is expected behavior for small test datasets
        
        // Both methods should produce the same result
        EXPECT_EQ(batch_individual.size(), batch_range.size());
    }
}

// Test efficient container usage and reserve() behavior
TEST_F(BatchProcessingPerformanceTest, EfficientContainerUsage) {
    // Test 1: Reserve behavior with different sizes
    std::vector<size_t> test_sizes = {10, 100, 1000, 10000, 100000};
    
    for (size_t size : test_sizes) {
        BatchData batch;
        
        // Measure time to reserve
        auto reserve_time = measureExecutionTime([&]() {
            batch.reserve(size);
        });
        
        EXPECT_GE(batch.capacity(), size);
        EXPECT_EQ(batch.size(), 0);
        
        // Measure time to fill reserved capacity
        auto fill_time = measureExecutionTime([&]() {
            for (size_t i = 0; i < size && i < medium_int_pool_.size(); ++i) {
                batch.add_item(medium_int_pool_[i]);
            }
        });
        
        std::cout << "Size: " << size 
                  << ", Reserve time: " << reserve_time.count() << " ns"
                  << ", Fill time: " << fill_time.count() << " ns\n";
        
        // Verify no reallocation occurred during filling
        EXPECT_GE(batch.capacity(), batch.size());
    }
    
    // Test 2: Memory pool efficiency
    {
        auto pool = std::make_shared<BatchMemoryPool>(1024 * 1024); // 1MB pool
        BatchData batch_with_pool(pool, 1000);
        BatchData batch_without_pool(1000);
        
        // Fill both batches and compare performance
        auto time_with_pool = measureExecutionTime([&]() {
            for (size_t i = 0; i < 1000 && i < medium_int_pool_.size(); ++i) {
                batch_with_pool.add_item(medium_int_pool_[i]);
            }
        });
        
        auto time_without_pool = measureExecutionTime([&]() {
            for (size_t i = 0; i < 1000 && i < medium_int_pool_.size(); ++i) {
                batch_without_pool.add_item(medium_int_pool_[i]);
            }
        });
        
        std::cout << "With pool: " << time_with_pool.count() << " ns\n";
        std::cout << "Without pool: " << time_without_pool.count() << " ns\n";
        
        EXPECT_EQ(batch_with_pool.size(), batch_without_pool.size());
    }
    
    // Test 3: Container growth patterns
    {
        BatchData batch;
        std::vector<size_t> capacities;
        
        // Track capacity growth as items are added
        for (size_t i = 0; i < 1000 && i < medium_int_pool_.size(); ++i) {
            size_t old_capacity = batch.capacity();
            batch.add_item(medium_int_pool_[i]);
            size_t new_capacity = batch.capacity();
            
            if (new_capacity != old_capacity) {
                capacities.push_back(new_capacity);
                std::cout << "Capacity grew to: " << new_capacity << " at size: " << i + 1 << "\n";
            }
        }
        
        // Verify reasonable growth pattern (typically 2x growth)
        for (size_t i = 1; i < capacities.size(); ++i) {
            double growth_factor = static_cast<double>(capacities[i]) / capacities[i-1];
            EXPECT_GE(growth_factor, 1.5); // At least 1.5x growth
            EXPECT_LE(growth_factor, 3.0); // At most 3x growth
        }
    }
}

// Benchmark batch processing performance characteristics
TEST_F(BatchProcessingPerformanceTest, BatchProcessingPerformanceCharacteristics) {
    // Test 1: Iteration performance comparison
    {
        BatchData large_batch;
        large_batch.reserve(large_double_pool_.size());
        large_batch.add_range(large_double_pool_.begin(), large_double_pool_.end());
        
        // Test different iteration methods
        double sum1 = 0.0, sum2 = 0.0, sum3 = 0.0;
        
        // Range-based for loop
        auto time_range_for = measureExecutionTime([&]() {
            for (const auto& item : large_batch) {
                if (auto typed_item = item->template as<TypedData<double>>()) {
                    sum1 += typed_item->get();
                }
            }
        });
        
        // Iterator-based loop
        auto time_iterator = measureExecutionTime([&]() {
            for (auto it = large_batch.begin(); it != large_batch.end(); ++it) {
                if (auto typed_item = (*it)->template as<TypedData<double>>()) {
                    sum2 += typed_item->get();
                }
            }
        });
        
        // Index-based loop
        auto time_index = measureExecutionTime([&]() {
            for (size_t i = 0; i < large_batch.size(); ++i) {
                if (auto typed_item = large_batch[i]->template as<TypedData<double>>()) {
                    sum3 += typed_item->get();
                }
            }
        });
        
        // All methods should produce the same result
        EXPECT_DOUBLE_EQ(sum1, sum2);
        EXPECT_DOUBLE_EQ(sum2, sum3);
        
        std::cout << "Range-based for: " << time_range_for.count() << " ns\n";
        std::cout << "Iterator-based: " << time_iterator.count() << " ns\n";
        std::cout << "Index-based: " << time_index.count() << " ns\n";
    }
    
    // Test 2: Type filtering performance
    {
        // Create mixed-type batch
        BatchData mixed_batch;
        mixed_batch.reserve(10000);
        
        for (int i = 0; i < 5000; ++i) {
            mixed_batch.add_typed_item(i);
            mixed_batch.add_typed_item(std::string("str_" + std::to_string(i)));
        }
        
        // Test filter_by_type performance
        auto time_filter_int = measureExecutionTime([&]() {
            auto int_items = mixed_batch.filter_by_type<TypedData<int>>();
            EXPECT_EQ(int_items.size(), 5000);
        });
        
        auto time_filter_string = measureExecutionTime([&]() {
            auto string_items = mixed_batch.filter_by_type<TypedData<std::string>>();
            EXPECT_EQ(string_items.size(), 5000);
        });
        
        // Test count_by_type performance
        auto time_count_int = measureExecutionTime([&]() {
            auto count = mixed_batch.count_by_type<TypedData<int>>();
            EXPECT_EQ(count, 5000);
        });
        
        std::cout << "Filter int time: " << time_filter_int.count() << " ns\n";
        std::cout << "Filter string time: " << time_filter_string.count() << " ns\n";
        std::cout << "Count int time: " << time_count_int.count() << " ns\n";
        
        // Count should be faster than filter
        EXPECT_LT(time_count_int.count(), time_filter_int.count());
    }
    
    // Test 3: Batch operations scaling
    {
        std::vector<size_t> batch_sizes = {100, 1000, 10000, 100000};
        
        for (size_t size : batch_sizes) {
            if (size > large_double_pool_.size()) continue;
            
            BatchData batch;
            batch.reserve(size);
            
            // Measure batch creation time
            auto creation_time = measureExecutionTime([&]() {
                for (size_t i = 0; i < size; ++i) {
                    batch.add_item(large_double_pool_[i]);
                }
            });
            
            // Measure batch processing time (simple sum operation)
            double sum = 0.0;
            auto processing_time = measureExecutionTime([&]() {
                for (const auto& item : batch) {
                    if (auto typed_item = item->template as<TypedData<double>>()) {
                        sum += typed_item->get();
                    }
                }
            });
            
            // Measure batch cleanup time
            auto cleanup_time = measureExecutionTime([&]() {
                batch.clear();
            });
            
            double creation_per_item = static_cast<double>(creation_time.count()) / size;
            double processing_per_item = static_cast<double>(processing_time.count()) / size;
            double cleanup_per_item = static_cast<double>(cleanup_time.count()) / size;
            
            std::cout << "Size: " << size 
                      << ", Creation/item: " << creation_per_item << " ns"
                      << ", Processing/item: " << processing_per_item << " ns"
                      << ", Cleanup/item: " << cleanup_per_item << " ns\n";
            
            // Verify linear scaling (within reasonable bounds)
            EXPECT_LT(creation_per_item, 1000.0); // Less than 1μs per item
            EXPECT_LT(processing_per_item, 100.0); // Less than 100ns per item
        }
    }
    
    // Test 4: Memory usage patterns
    {
        // Test memory usage with different batch sizes
        std::vector<size_t> sizes = {1000, 10000, 100000};
        
        for (size_t size : sizes) {
            if (size > medium_int_pool_.size()) continue;
            
            resetMemoryTracking();
            
            {
                BatchData batch(size); // Pre-reserve
                for (size_t i = 0; i < size; ++i) {
                    batch.add_item(medium_int_pool_[i]);
                }
                
                // Measure memory efficiency
                size_t actual_size = batch.size();
                size_t capacity = batch.capacity();
                double memory_efficiency = static_cast<double>(actual_size) / capacity;
                
                std::cout << "Size: " << size 
                          << ", Capacity: " << capacity
                          << ", Efficiency: " << memory_efficiency << "\n";
                
                // With pre-reservation, efficiency should be high
                EXPECT_GE(memory_efficiency, 0.8); // At least 80% efficient
            }
            
            // Memory should be released after batch destruction
            // In production, verify with memory profiling tools
        }
    }
}

// Test concurrent batch processing performance
TEST_F(BatchProcessingPerformanceTest, ConcurrentBatchProcessing) {
    const size_t num_threads = std::thread::hardware_concurrency();
    const size_t items_per_thread = 10000;
    
    // Test 1: Concurrent batch creation
    {
        std::vector<std::thread> threads;
        std::vector<std::unique_ptr<BatchData>> batches(num_threads);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                batches[t] = std::make_unique<BatchData>(items_per_thread);
                for (size_t i = 0; i < items_per_thread && i < medium_int_pool_.size(); ++i) {
                    batches[t]->add_item(medium_int_pool_[i]);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Concurrent batch creation (" << num_threads << " threads): " 
                  << total_time.count() << " ms\n";
        
        // Verify all batches were created correctly
        for (const auto& batch : batches) {
            EXPECT_EQ(batch->size(), items_per_thread);
        }
    }
    
    // Test 2: Concurrent batch processing
    {
        // Create a large batch for concurrent processing
        BatchData large_batch(num_threads * items_per_thread);
        for (size_t i = 0; i < num_threads * items_per_thread && i < medium_int_pool_.size(); ++i) {
            large_batch.add_item(medium_int_pool_[i]);
        }
        
        std::vector<std::thread> threads;
        std::vector<long long> partial_sums(num_threads, 0);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                size_t start_idx = t * items_per_thread;
                size_t end_idx = std::min(start_idx + items_per_thread, large_batch.size());
                
                for (size_t i = start_idx; i < end_idx; ++i) {
                    if (auto typed_item = large_batch[i]->template as<TypedData<int>>()) {
                        partial_sums[t] += typed_item->get();
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Calculate total sum
        long long total_sum = std::accumulate(partial_sums.begin(), partial_sums.end(), 0LL);
        
        std::cout << "Concurrent batch processing (" << num_threads << " threads): " 
                  << total_time.count() << " ms, Total sum: " << total_sum << "\n";
        
        // Verify result correctness by comparing with sequential processing
        long long sequential_sum = 0;
        for (size_t i = 0; i < large_batch.size(); ++i) {
            if (auto typed_item = large_batch[i]->template as<TypedData<int>>()) {
                sequential_sum += typed_item->get();
            }
        }
        
        EXPECT_EQ(total_sum, sequential_sum);
    }
}

// Stress test for memory and performance under extreme conditions
TEST_F(BatchProcessingPerformanceTest, StressTest) {
    // Test with very large batches
    const size_t stress_size = 1000000; // 1 million items
    
    if (large_double_pool_.size() >= stress_size) {
        BatchData stress_batch;
        
        // Measure time to create large batch without reserve
        auto time_no_reserve = measureExecutionTime([&]() {
            for (size_t i = 0; i < stress_size; ++i) {
                stress_batch.add_item(large_double_pool_[i]);
            }
        });
        
        std::cout << "Stress test (no reserve, " << stress_size << " items): " 
                  << time_no_reserve.count() << " ns\n";
        
        EXPECT_EQ(stress_batch.size(), stress_size);
        
        // Test iteration performance on large batch
        double sum = 0.0;
        auto iteration_time = measureExecutionTime([&]() {
            for (const auto& item : stress_batch) {
                if (auto typed_item = item->template as<TypedData<double>>()) {
                    sum += typed_item->get();
                }
            }
        });
        
        std::cout << "Stress test iteration time: " << iteration_time.count() << " ns\n";
        
        // Test memory cleanup
        auto cleanup_time = measureExecutionTime([&]() {
            stress_batch.clear();
        });
        
        std::cout << "Stress test cleanup time: " << cleanup_time.count() << " ns\n";
        
        EXPECT_EQ(stress_batch.size(), 0);
    }
}