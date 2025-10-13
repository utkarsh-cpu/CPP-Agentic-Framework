#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include "../AsyncNode/AsyncNode.h"
#include "../AsyncFlow/AsyncFlow.h"
#include "../AsyncBatchFlow/AsyncBatchFlow.h"
#include "../AsyncParallelBatchFlow/AsyncParallelBatchFlow.h"
#include "../BaseNode/OptimizedBatchProcessor.h"
#include "../BaseNode/BatchData.h"
#include "../BaseNode/TypedData.h"
#include "../BaseNode/CancellationToken.h"
#include "../BaseNode/AsyncTimeout.h"
#include "../BaseNode/FrameworkError.h"
#include "../BaseNode/Result.h"
#include "../Flow/StringData.h"
#include "../BatchFlow/MapData.h"
#include "../BatchNode/VectorData.h"
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <future>
#include <random>
#include <atomic>
#include <algorithm>
#include <numeric>

using framework::Result;
using framework::FrameworkError;
using framework::ErrorCategory;

// Test fixture for integration workflow tests
class IntegrationWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize random number generator for consistent tests
        rng_.seed(42);
        
        // Create test data pools
        createTestDataPools();
        
        // Initialize performance tracking
        resetPerformanceCounters();
    }
    
    void TearDown() override {
        // Verify no resource leaks
        verifyResourceCleanup();
    }
    
    void createTestDataPools() {
        // Small dataset for basic workflow tests
        small_dataset_.reserve(100);
        for (int i = 0; i < 100; ++i) {
            small_dataset_.push_back(
                std::make_shared<TypedData<int>>(i)
            );
        }
        
        // Medium dataset for performance tests
        medium_dataset_.reserve(10000);
        for (int i = 0; i < 10000; ++i) {
            medium_dataset_.push_back(
                std::make_shared<TypedData<double>>(static_cast<double>(i) * 0.1)
            );
        }
        
        // Large dataset for stress tests
        large_dataset_.reserve(100000);
        std::uniform_real_distribution<float> dist(0.0f, 1000.0f);
        for (int i = 0; i < 100000; ++i) {
            large_dataset_.push_back(
                std::make_shared<TypedData<float>>(dist(rng_))
            );
        }
    }
    
    void resetPerformanceCounters() {
        total_processing_time_ = std::chrono::nanoseconds{0};
        items_processed_ = 0;
        errors_encountered_ = 0;
    }
    
    void verifyResourceCleanup() {
        // In production, integrate with memory profiling tools
        // For now, just verify basic cleanup
        EXPECT_EQ(active_workflows_.load(), 0);
    }
    
    template<typename Func>
    std::chrono::nanoseconds measureExecutionTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    }
    
protected:
    std::mt19937 rng_;
    std::vector<std::shared_ptr<FrameworkData>> small_dataset_;
    std::vector<std::shared_ptr<FrameworkData>> medium_dataset_;
    std::vector<std::shared_ptr<FrameworkData>> large_dataset_;
    
    // Performance tracking
    std::chrono::nanoseconds total_processing_time_{0};
    std::atomic<size_t> items_processed_{0};
    std::atomic<size_t> errors_encountered_{0};
    std::atomic<size_t> active_workflows_{0};
};

// Test nodes for workflow scenarios
class DataProcessingNode : public AsyncNode {
private:
    std::string operation_name_;
    std::chrono::milliseconds processing_delay_;
    bool should_fail_;
    double failure_probability_;
    std::mt19937 rng_;
    
public:
    explicit DataProcessingNode(
        std::string operation_name,
        std::chrono::milliseconds delay = std::chrono::milliseconds(10),
        bool should_fail = false,
        double failure_prob = 0.0
    ) : operation_name_(std::move(operation_name))
      , processing_delay_(delay)
      , should_fail_(should_fail)
      , failure_probability_(failure_prob)
      , rng_(std::random_device{}()) {}
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes) override {
        return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
            // Simulate processing delay
            if (processing_delay_.count() > 0) {
                std::this_thread::sleep_for(processing_delay_);
            }
            
            // Simulate random failures
            if (failure_probability_ > 0.0) {
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                if (dist(rng_) < failure_probability_) {
                    throw std::runtime_error("Random failure in " + operation_name_);
                }
            }
            
            // Simulate deterministic failure
            if (should_fail_) {
                throw std::runtime_error("Intentional failure in " + operation_name_);
            }
            
            // Process the data based on type
            if (auto int_data = PrepRes->as<TypedData<int>>()) {
                int value = int_data->get();
                return std::make_shared<TypedData<int>>(value * 2); // Double the value
            } else if (auto double_data = PrepRes->as<TypedData<double>>()) {
                double value = double_data->get();
                return std::make_shared<TypedData<double>>(value + 1.0); // Add 1
            } else if (auto float_data = PrepRes->as<TypedData<float>>()) {
                float value = float_data->get();
                return std::make_shared<TypedData<float>>(value * 0.5f); // Half the value
            } else if (auto string_data = PrepRes->as<StringData>()) {
                std::string value = string_data->value;
                return std::make_shared<StringData>(operation_name_ + "_" + value);
            }
            
            // Return processed marker for unknown types
            return std::make_shared<StringData>(operation_name_ + "_processed");
        });
    }
    
    void set_failure_probability(double prob) {
        failure_probability_ = prob;
    }
    
    void set_processing_delay(std::chrono::milliseconds delay) {
        processing_delay_ = delay;
    }
};

class AggregationNode : public AsyncNode {
private:
    std::string aggregation_type_;
    
public:
    explicit AggregationNode(std::string agg_type = "sum") 
        : aggregation_type_(std::move(agg_type)) {}
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes) override {
        return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
            if (auto batch_data = PrepRes->as<BatchData>()) {
                if (aggregation_type_ == "sum") {
                    return performSum(*batch_data);
                } else if (aggregation_type_ == "count") {
                    return performCount(*batch_data);
                } else if (aggregation_type_ == "average") {
                    return performAverage(*batch_data);
                }
            }
            
            return std::make_shared<StringData>("aggregation_failed");
        });
    }
    
private:
    std::shared_ptr<FrameworkData> performSum(const BatchData& batch) {
        double sum = 0.0;
        size_t count = 0;
        
        for (const auto& item : batch) {
            if (auto int_data = item->as<TypedData<int>>()) {
                sum += int_data->get();
                count++;
            } else if (auto double_data = item->as<TypedData<double>>()) {
                sum += double_data->get();
                count++;
            } else if (auto float_data = item->as<TypedData<float>>()) {
                sum += float_data->get();
                count++;
            }
        }
        
        return std::make_shared<TypedData<double>>(sum);
    }
    
    std::shared_ptr<FrameworkData> performCount(const BatchData& batch) {
        return std::make_shared<TypedData<size_t>>(batch.size());
    }
    
    std::shared_ptr<FrameworkData> performAverage(const BatchData& batch) {
        double sum = 0.0;
        size_t count = 0;
        
        for (const auto& item : batch) {
            if (auto int_data = item->as<TypedData<int>>()) {
                sum += int_data->get();
                count++;
            } else if (auto double_data = item->as<TypedData<double>>()) {
                sum += double_data->get();
                count++;
            } else if (auto float_data = item->as<TypedData<float>>()) {
                sum += float_data->get();
                count++;
            }
        }
        
        if (count == 0) {
            return std::make_shared<TypedData<double>>(0.0);
        }
        
        return std::make_shared<TypedData<double>>(sum / count);
    }
};

// Test 1: Complete data processing pipelines with multiple nodes
TEST_F(IntegrationWorkflowTest, CompleteDataProcessingPipeline) {
    std::cout << "\n=== Testing Complete Data Processing Pipeline ===\n";
    
    // Create a multi-stage processing pipeline
    auto input_node = std::make_shared<DataProcessingNode>("input_processor");
    auto transform_node = std::make_shared<DataProcessingNode>("transformer");
    auto aggregation_node = std::make_shared<AggregationNode>("sum");
    auto output_node = std::make_shared<DataProcessingNode>("output_processor");
    
    // Create workflow
    auto workflow = std::make_shared<AsyncFlow>(input_node);
    
    // Chain the nodes together
    input_node->next(transform_node);
    transform_node->next(aggregation_node);
    aggregation_node->next(output_node);
    
    // Test with small dataset
    {
        active_workflows_.fetch_add(1);
        
        // Create batch input
        auto input_batch = std::make_unique<BatchData>();
        for (const auto& item : small_dataset_) {
            input_batch->add_item(item);
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // Execute the workflow
            auto future = workflow->RunAsync(std::move(input_batch));
            auto result = future.get();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            EXPECT_NE(result, nullptr);
            std::cout << "Pipeline completed successfully in " << execution_time.count() << " ms\n";
            
            // Verify result type and content
            if (auto final_result = result->as<TypedData<double>>()) {
                double sum_value = final_result->get();
                std::cout << "Final aggregated result: " << sum_value << "\n";
                EXPECT_GT(sum_value, 0.0); // Should be positive sum
            }
            
            items_processed_.fetch_add(small_dataset_.size());
            
        } catch (const std::exception& e) {
            std::cout << "Pipeline failed: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
            FAIL() << "Pipeline should not fail with valid data";
        }
        
        active_workflows_.fetch_sub(1);
    }
    
    // Test with medium dataset for performance
    {
        active_workflows_.fetch_add(1);
        
        auto input_batch = std::make_unique<BatchData>();
        for (const auto& item : medium_dataset_) {
            input_batch->add_item(item);
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            auto future = workflow->RunAsync(std::move(input_batch));
            auto result = future.get();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            EXPECT_NE(result, nullptr);
            std::cout << "Medium dataset pipeline completed in " << execution_time.count() << " ms\n";
            
            items_processed_.fetch_add(medium_dataset_.size());
            
        } catch (const std::exception& e) {
            std::cout << "Medium dataset pipeline failed: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
            FAIL() << "Pipeline should handle medium datasets";
        }
        
        active_workflows_.fetch_sub(1);
    }
    
    std::cout << "Total items processed: " << items_processed_.load() << "\n";
    std::cout << "Errors encountered: " << errors_encountered_.load() << "\n";
}

// Test 2: Async workflow execution with proper error handling
TEST_F(IntegrationWorkflowTest, AsyncWorkflowWithErrorHandling) {
    std::cout << "\n=== Testing Async Workflow Error Handling ===\n";
    
    // Test 1: Error propagation through workflow
    {
        auto failing_node = std::make_shared<DataProcessingNode>("failing_node", 
                                                                std::chrono::milliseconds(50), 
                                                                true); // Will fail
        auto recovery_node = std::make_shared<DataProcessingNode>("recovery_node");
        
        auto workflow = std::make_shared<AsyncFlow>(failing_node);
        failing_node->next(recovery_node);
        
        auto input_data = std::make_shared<TypedData<int>>(42);
        
        try {
            auto future = workflow->RunAsync(input_data);
            auto result = future.get();
            
            // Should not reach here if error handling is working
            FAIL() << "Expected exception was not thrown";
            
        } catch (const std::exception& e) {
            std::cout << "Expected error caught: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
            EXPECT_TRUE(std::string(e.what()).find("failing_node") != std::string::npos);
        }
    }
    
    // Test 2: Timeout handling in async workflows
    {
        auto slow_node = std::make_shared<DataProcessingNode>("slow_node", 
                                                             std::chrono::seconds(5)); // 5 second delay
        auto workflow = std::make_shared<AsyncFlow>(slow_node);
        
        auto input_data = std::make_shared<TypedData<int>>(42);
        
        try {
            // Use timeout of 2 seconds
            auto future = workflow->RunAsyncWithTimeout(input_data, std::chrono::seconds(2));
            auto result = future.get();
            
            FAIL() << "Expected timeout exception was not thrown";
            
        } catch (const TimeoutException& e) {
            std::cout << "Timeout handled correctly: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
        } catch (const std::exception& e) {
            std::cout << "Unexpected error during timeout test: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
        }
    }
    
    // Test 3: Cancellation handling
    {
        auto long_running_node = std::make_shared<DataProcessingNode>("long_running", 
                                                                     std::chrono::seconds(10));
        auto workflow = std::make_shared<AsyncFlow>(long_running_node);
        
        CancellationTokenSource token_source;
        auto input_data = std::make_shared<TypedData<int>>(42);
        
        try {
            auto future = workflow->RunAsyncWithCancellation(input_data, token_source.token());
            
            // Cancel after 1 second
            std::thread canceller([&token_source]() {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                token_source.cancel();
            });
            
            auto result = future.get();
            canceller.join();
            
            FAIL() << "Expected cancellation exception was not thrown";
            
        } catch (const OperationCancelledException& e) {
            std::cout << "Cancellation handled correctly: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
        } catch (const std::exception& e) {
            std::cout << "Unexpected error during cancellation test: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
        }
    }
    
    // Test 4: Random failure handling with retry logic
    {
        auto unreliable_node = std::make_shared<DataProcessingNode>("unreliable_node", 
                                                                   std::chrono::milliseconds(10),
                                                                   false,
                                                                   0.3); // 30% failure rate
        auto workflow = std::make_shared<AsyncFlow>(unreliable_node);
        
        size_t success_count = 0;
        size_t failure_count = 0;
        const size_t test_iterations = 100;
        
        for (size_t i = 0; i < test_iterations; ++i) {
            auto input_data = std::make_shared<TypedData<int>>(static_cast<int>(i));
            
            try {
                auto future = workflow->RunAsync(input_data);
                auto result = future.get();
                
                if (result) {
                    success_count++;
                }
                
            } catch (const std::exception& e) {
                failure_count++;
                errors_encountered_.fetch_add(1);
            }
        }
        
        std::cout << "Reliability test: " << success_count << " successes, " 
                  << failure_count << " failures out of " << test_iterations << " attempts\n";
        
        // With 30% failure rate, we expect roughly 70% success rate
        double success_rate = static_cast<double>(success_count) / test_iterations;
        EXPECT_GT(success_rate, 0.5); // At least 50% should succeed
        EXPECT_LT(success_rate, 0.9); // But not more than 90% (due to random failures)
    }
    
    std::cout << "Total errors handled: " << errors_encountered_.load() << "\n";
}

// Test 3: Batch processing workflows with large datasets
TEST_F(IntegrationWorkflowTest, BatchProcessingWorkflowsLargeDatasets) {
    std::cout << "\n=== Testing Batch Processing Workflows with Large Datasets ===\n";
    
    // Test 1: Large dataset batch processing with OptimizedBatchProcessor
    {
        auto processor = create_optimized_processor(
            std::thread::hardware_concurrency(), 
            true // Enable SIMD
        );
        
        // Create large batch from dataset
        auto large_batch = std::make_unique<BatchData>();
        large_batch->reserve(large_dataset_.size());
        for (const auto& item : large_dataset_) {
            large_batch->add_item(item);
        }
        
        std::cout << "Processing batch with " << large_batch->size() << " items\n";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Define a complex processing function
        auto complex_processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
            if (auto float_data = data->as<TypedData<float>>()) {
                float value = float_data->get();
                
                // Simulate complex mathematical operations
                float result = value;
                for (int i = 0; i < 10; ++i) {
                    result = std::sin(result) * std::cos(result) + value * 0.1f;
                }
                
                return std::make_shared<TypedData<float>>(result);
            }
            return nullptr;
        };
        
        try {
            auto result = processor->process_batch(*large_batch, complex_processor);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            EXPECT_TRUE(result.has_value());
            if (result.has_value()) {
                auto& output_batch = result.value();
                EXPECT_EQ(output_batch->size(), large_batch->size());
                
                std::cout << "Large batch processing completed in " << execution_time.count() << " ms\n";
                std::cout << "Throughput: " << (large_batch->size() * 1000.0 / execution_time.count()) 
                          << " items/second\n";
                std::cout << "Work stealing events: " << processor->work_stealing_count() << "\n";
                std::cout << "Pool efficiency: " << processor->work_item_pool_efficiency() << "%\n";
                
                items_processed_.fetch_add(large_batch->size());
            }
            
        } catch (const std::exception& e) {
            std::cout << "Large batch processing failed: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
            FAIL() << "Large batch processing should succeed";
        }
    }
    
    // Test 2: AsyncBatchFlow with large datasets
    {
        auto batch_processor_node = std::make_shared<DataProcessingNode>("batch_processor", 
                                                                        std::chrono::milliseconds(1));
        auto aggregation_node = std::make_shared<AggregationNode>("average");
        
        auto batch_workflow = std::make_shared<AsyncBatchFlow>(batch_processor_node);
        batch_processor_node->next(aggregation_node);
        
        // Create medium-sized batch for async batch flow
        auto medium_batch = std::make_unique<BatchData>();
        medium_batch->reserve(medium_dataset_.size());
        for (const auto& item : medium_dataset_) {
            medium_batch->add_item(item);
        }
        
        std::cout << "AsyncBatchFlow processing " << medium_batch->size() << " items\n";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            auto future = batch_workflow->RunAsync(std::move(medium_batch));
            auto result = future.get();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            EXPECT_NE(result, nullptr);
            
            std::cout << "AsyncBatchFlow completed in " << execution_time.count() << " ms\n";
            
            if (auto avg_result = result->as<TypedData<double>>()) {
                std::cout << "Average result: " << avg_result->get() << "\n";
                EXPECT_GT(avg_result->get(), 0.0);
            }
            
            items_processed_.fetch_add(medium_dataset_.size());
            
        } catch (const std::exception& e) {
            std::cout << "AsyncBatchFlow failed: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
            FAIL() << "AsyncBatchFlow should handle medium datasets";
        }
    }
    
    // Test 3: AsyncParallelBatchFlow for maximum performance
    {
        auto parallel_processor_node = std::make_shared<DataProcessingNode>("parallel_processor", 
                                                                           std::chrono::milliseconds(2));
        auto final_aggregation_node = std::make_shared<AggregationNode>("sum");
        
        auto parallel_workflow = std::make_shared<AsyncParallelBatchFlow>(parallel_processor_node);
        parallel_processor_node->next(final_aggregation_node);
        
        // Use large dataset for parallel processing
        auto parallel_batch = std::make_unique<BatchData>();
        parallel_batch->reserve(large_dataset_.size());
        for (const auto& item : large_dataset_) {
            parallel_batch->add_item(item);
        }
        
        std::cout << "AsyncParallelBatchFlow processing " << parallel_batch->size() << " items\n";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            auto future = parallel_workflow->RunAsync(std::move(parallel_batch));
            auto result = future.get();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            EXPECT_NE(result, nullptr);
            
            std::cout << "AsyncParallelBatchFlow completed in " << execution_time.count() << " ms\n";
            std::cout << "Parallel throughput: " << (large_dataset_.size() * 1000.0 / execution_time.count()) 
                      << " items/second\n";
            
            if (auto sum_result = result->as<TypedData<double>>()) {
                std::cout << "Sum result: " << sum_result->get() << "\n";
                EXPECT_GT(sum_result->get(), 0.0);
            }
            
            items_processed_.fetch_add(large_dataset_.size());
            
        } catch (const std::exception& e) {
            std::cout << "AsyncParallelBatchFlow failed: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
            FAIL() << "AsyncParallelBatchFlow should handle large datasets efficiently";
        }
    }
    
    // Test 4: Memory efficiency with very large batches
    {
        const size_t very_large_size = 500000; // Half million items
        
        if (very_large_size <= large_dataset_.size()) {
            auto memory_efficient_batch = std::make_unique<BatchData>();
            memory_efficient_batch->reserve(very_large_size);
            
            // Use memory pool for efficiency
            auto memory_pool = std::make_shared<BatchMemoryPool>(1024 * 1024 * 10); // 10MB pool
            auto pooled_batch = std::make_unique<BatchData>(memory_pool, very_large_size);
            
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Fill the batch
            for (size_t i = 0; i < very_large_size; ++i) {
                pooled_batch->add_item(large_dataset_[i]);
            }
            
            auto fill_time = std::chrono::high_resolution_clock::now();
            auto fill_duration = std::chrono::duration_cast<std::chrono::milliseconds>(fill_time - start_time);
            
            std::cout << "Filled " << very_large_size << " items in " << fill_duration.count() << " ms\n";
            
            // Process with simple operation
            size_t processed_count = 0;
            for (const auto& item : *pooled_batch) {
                if (item) {
                    processed_count++;
                }
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            EXPECT_EQ(processed_count, very_large_size);
            std::cout << "Total processing time for " << very_large_size << " items: " 
                      << total_duration.count() << " ms\n";
            
            items_processed_.fetch_add(very_large_size);
        }
    }
    
    std::cout << "Total items processed in batch tests: " << items_processed_.load() << "\n";
}

// Test 4: Performance benchmarks for realistic usage scenarios
TEST_F(IntegrationWorkflowTest, PerformanceBenchmarksRealisticScenarios) {
    std::cout << "\n=== Performance Benchmarks for Realistic Usage Scenarios ===\n";
    
    // Scenario 1: Real-time data processing pipeline
    {
        std::cout << "\n--- Real-time Data Processing Scenario ---\n";
        
        auto input_validator = std::make_shared<DataProcessingNode>("validator", 
                                                                   std::chrono::milliseconds(1));
        auto data_transformer = std::make_shared<DataProcessingNode>("transformer", 
                                                                    std::chrono::milliseconds(5));
        auto data_enricher = std::make_shared<DataProcessingNode>("enricher", 
                                                                 std::chrono::milliseconds(3));
        auto output_formatter = std::make_shared<DataProcessingNode>("formatter", 
                                                                    std::chrono::milliseconds(2));
        
        // Create real-time processing workflow
        auto realtime_workflow = std::make_shared<AsyncFlow>(input_validator);
        input_validator->next(data_transformer);
        data_transformer->next(data_enricher);
        data_enricher->next(output_formatter);
        
        // Simulate real-time processing with multiple concurrent requests
        const size_t concurrent_requests = 100;
        const size_t items_per_request = 10;
        
        std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
        futures.reserve(concurrent_requests);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Submit concurrent requests
        for (size_t i = 0; i < concurrent_requests; ++i) {
            auto request_batch = std::make_unique<BatchData>();
            for (size_t j = 0; j < items_per_request; ++j) {
                size_t index = (i * items_per_request + j) % small_dataset_.size();
                request_batch->add_item(small_dataset_[index]);
            }
            
            futures.emplace_back(realtime_workflow->RunAsync(std::move(request_batch)));
        }
        
        // Wait for all requests to complete
        size_t successful_requests = 0;
        for (auto& future : futures) {
            try {
                auto result = future.get();
                if (result) {
                    successful_requests++;
                }
            } catch (const std::exception& e) {
                errors_encountered_.fetch_add(1);
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        double requests_per_second = (concurrent_requests * 1000.0) / total_duration.count();
        double items_per_second = (concurrent_requests * items_per_request * 1000.0) / total_duration.count();
        
        std::cout << "Real-time processing results:\n";
        std::cout << "  - Total requests: " << concurrent_requests << "\n";
        std::cout << "  - Successful requests: " << successful_requests << "\n";
        std::cout << "  - Total duration: " << total_duration.count() << " ms\n";
        std::cout << "  - Requests/second: " << requests_per_second << "\n";
        std::cout << "  - Items/second: " << items_per_second << "\n";
        
        EXPECT_GT(successful_requests, concurrent_requests * 0.95); // At least 95% success rate
        items_processed_.fetch_add(successful_requests * items_per_request);
    }
    
    // Scenario 2: Batch analytics processing
    {
        std::cout << "\n--- Batch Analytics Processing Scenario ---\n";
        
        auto data_cleaner = std::make_shared<DataProcessingNode>("cleaner", 
                                                                std::chrono::milliseconds(2));
        auto statistical_analyzer = std::make_shared<AggregationNode>("average");
        auto report_generator = std::make_shared<DataProcessingNode>("reporter", 
                                                                    std::chrono::milliseconds(10));
        
        auto analytics_workflow = std::make_shared<AsyncBatchFlow>(data_cleaner);
        data_cleaner->next(statistical_analyzer);
        statistical_analyzer->next(report_generator);
        
        // Process multiple large batches sequentially
        const size_t num_batches = 5;
        const size_t batch_size = medium_dataset_.size() / num_batches;
        
        std::vector<double> batch_processing_times;
        batch_processing_times.reserve(num_batches);
        
        for (size_t batch_idx = 0; batch_idx < num_batches; ++batch_idx) {
            auto analytics_batch = std::make_unique<BatchData>();
            analytics_batch->reserve(batch_size);
            
            size_t start_idx = batch_idx * batch_size;
            size_t end_idx = std::min(start_idx + batch_size, medium_dataset_.size());
            
            for (size_t i = start_idx; i < end_idx; ++i) {
                analytics_batch->add_item(medium_dataset_[i]);
            }
            
            auto batch_start = std::chrono::high_resolution_clock::now();
            
            try {
                auto future = analytics_workflow->RunAsync(std::move(analytics_batch));
                auto result = future.get();
                
                auto batch_end = std::chrono::high_resolution_clock::now();
                auto batch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(batch_end - batch_start);
                
                batch_processing_times.push_back(batch_duration.count());
                
                EXPECT_NE(result, nullptr);
                items_processed_.fetch_add(end_idx - start_idx);
                
            } catch (const std::exception& e) {
                std::cout << "Batch " << batch_idx << " failed: " << e.what() << "\n";
                errors_encountered_.fetch_add(1);
            }
        }
        
        // Calculate statistics
        if (!batch_processing_times.empty()) {
            double avg_time = std::accumulate(batch_processing_times.begin(), 
                                            batch_processing_times.end(), 0.0) / batch_processing_times.size();
            double min_time = *std::min_element(batch_processing_times.begin(), batch_processing_times.end());
            double max_time = *std::max_element(batch_processing_times.begin(), batch_processing_times.end());
            
            std::cout << "Batch analytics processing results:\n";
            std::cout << "  - Number of batches: " << num_batches << "\n";
            std::cout << "  - Average batch size: " << batch_size << "\n";
            std::cout << "  - Average processing time: " << avg_time << " ms\n";
            std::cout << "  - Min processing time: " << min_time << " ms\n";
            std::cout << "  - Max processing time: " << max_time << " ms\n";
            std::cout << "  - Throughput: " << (batch_size * 1000.0 / avg_time) << " items/second\n";
        }
    }
    
    // Scenario 3: High-throughput streaming processing
    {
        std::cout << "\n--- High-throughput Streaming Processing Scenario ---\n";
        
        auto stream_processor = std::make_shared<DataProcessingNode>("stream_processor", 
                                                                    std::chrono::milliseconds(1));
        auto stream_aggregator = std::make_shared<AggregationNode>("count");
        
        auto streaming_workflow = std::make_shared<AsyncParallelBatchFlow>(stream_processor);
        stream_processor->next(stream_aggregator);
        
        // Simulate high-throughput streaming with small batches processed rapidly
        const size_t num_stream_batches = 1000;
        const size_t stream_batch_size = 50;
        
        std::atomic<size_t> completed_batches{0};
        std::atomic<size_t> failed_batches{0};
        
        auto streaming_start = std::chrono::high_resolution_clock::now();
        
        // Process batches concurrently
        std::vector<std::thread> streaming_threads;
        const size_t num_streaming_threads = std::min(static_cast<size_t>(8), 
                                                     static_cast<size_t>(std::thread::hardware_concurrency()));
        
        for (size_t thread_id = 0; thread_id < num_streaming_threads; ++thread_id) {
            streaming_threads.emplace_back([&, thread_id]() {
                size_t batches_per_thread = num_stream_batches / num_streaming_threads;
                size_t start_batch = thread_id * batches_per_thread;
                size_t end_batch = (thread_id == num_streaming_threads - 1) ? 
                                  num_stream_batches : start_batch + batches_per_thread;
                
                for (size_t batch_idx = start_batch; batch_idx < end_batch; ++batch_idx) {
                    auto stream_batch = std::make_unique<BatchData>();
                    stream_batch->reserve(stream_batch_size);
                    
                    for (size_t i = 0; i < stream_batch_size; ++i) {
                        size_t data_idx = (batch_idx * stream_batch_size + i) % small_dataset_.size();
                        stream_batch->add_item(small_dataset_[data_idx]);
                    }
                    
                    try {
                        auto future = streaming_workflow->RunAsync(std::move(stream_batch));
                        auto result = future.get();
                        
                        if (result) {
                            completed_batches.fetch_add(1);
                        }
                        
                    } catch (const std::exception& e) {
                        failed_batches.fetch_add(1);
                        errors_encountered_.fetch_add(1);
                    }
                }
            });
        }
        
        // Wait for all streaming threads to complete
        for (auto& thread : streaming_threads) {
            thread.join();
        }
        
        auto streaming_end = std::chrono::high_resolution_clock::now();
        auto streaming_duration = std::chrono::duration_cast<std::chrono::milliseconds>(streaming_end - streaming_start);
        
        double batches_per_second = (completed_batches.load() * 1000.0) / streaming_duration.count();
        double items_per_second = (completed_batches.load() * stream_batch_size * 1000.0) / streaming_duration.count();
        
        std::cout << "High-throughput streaming results:\n";
        std::cout << "  - Total batches: " << num_stream_batches << "\n";
        std::cout << "  - Completed batches: " << completed_batches.load() << "\n";
        std::cout << "  - Failed batches: " << failed_batches.load() << "\n";
        std::cout << "  - Total duration: " << streaming_duration.count() << " ms\n";
        std::cout << "  - Batches/second: " << batches_per_second << "\n";
        std::cout << "  - Items/second: " << items_per_second << "\n";
        
        EXPECT_GT(completed_batches.load(), num_stream_batches * 0.95); // At least 95% success rate
        items_processed_.fetch_add(completed_batches.load() * stream_batch_size);
    }
    
    std::cout << "\nOverall Performance Summary:\n";
    std::cout << "  - Total items processed: " << items_processed_.load() << "\n";
    std::cout << "  - Total errors encountered: " << errors_encountered_.load() << "\n";
    std::cout << "  - Error rate: " << (static_cast<double>(errors_encountered_.load()) / items_processed_.load() * 100.0) << "%\n";
}

// Benchmark functions for Google Benchmark integration
static void BM_SimpleWorkflowExecution(benchmark::State& state) {
    auto processor_node = std::make_shared<DataProcessingNode>("benchmark_processor", 
                                                              std::chrono::milliseconds(1));
    auto workflow = std::make_shared<AsyncFlow>(processor_node);
    
    for (auto _ : state) {
        auto input_data = std::make_shared<TypedData<int>>(42);
        
        try {
            auto future = workflow->RunAsync(input_data);
            auto result = future.get();
            benchmark::DoNotOptimize(result);
        } catch (const std::exception& e) {
            state.SkipWithError(e.what());
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SimpleWorkflowExecution)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

static void BM_BatchProcessingThroughput(benchmark::State& state) {
    const size_t batch_size = static_cast<size_t>(state.range(0));
    
    auto processor = create_optimized_processor(std::thread::hardware_concurrency(), true);
    
    // Create test batch
    auto test_batch = std::make_unique<BatchData>();
    test_batch->reserve(batch_size);
    for (size_t i = 0; i < batch_size; ++i) {
        test_batch->add_typed_item(static_cast<float>(i));
    }
    
    auto simple_processor = [](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
        if (auto float_data = data->as<TypedData<float>>()) {
            float value = float_data->get();
            return std::make_shared<TypedData<float>>(value * 2.0f);
        }
        return nullptr;
    };
    
    for (auto _ : state) {
        try {
            auto result = processor->process_batch(*test_batch, simple_processor);
            if (result.has_value()) {
                benchmark::DoNotOptimize(result.value());
            } else {
                state.SkipWithError("Batch processing failed");
            }
        } catch (const std::exception& e) {
            state.SkipWithError(e.what());
        }
    }
    
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * batch_size));
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * batch_size * sizeof(float)));
}
BENCHMARK(BM_BatchProcessingThroughput)
    ->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000)
    ->Unit(benchmark::kMillisecond);

static void BM_ConcurrentWorkflowExecution(benchmark::State& state) {
    const size_t num_concurrent = static_cast<size_t>(state.range(0));
    
    auto processor_node = std::make_shared<DataProcessingNode>("concurrent_processor", 
                                                              std::chrono::milliseconds(1));
    auto workflow = std::make_shared<AsyncFlow>(processor_node);
    
    for (auto _ : state) {
        std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
        futures.reserve(num_concurrent);
        
        // Submit concurrent requests
        for (size_t i = 0; i < num_concurrent; ++i) {
            auto input_data = std::make_shared<TypedData<int>>(static_cast<int>(i));
            futures.emplace_back(workflow->RunAsync(input_data));
        }
        
        // Wait for all to complete
        for (auto& future : futures) {
            try {
                auto result = future.get();
                benchmark::DoNotOptimize(result);
            } catch (const std::exception& e) {
                state.SkipWithError(e.what());
            }
        }
    }
    
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * num_concurrent));
}
BENCHMARK(BM_ConcurrentWorkflowExecution)
    ->Arg(1)->Arg(10)->Arg(50)->Arg(100)
    ->Unit(benchmark::kMillisecond);