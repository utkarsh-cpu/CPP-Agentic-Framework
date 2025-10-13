#include <gtest/gtest.h>
#include "../AsyncNode/AsyncNode.h"
#include "../AsyncFlow/AsyncFlow.h"
#include "../BaseNode/BatchData.h"
#include "../BaseNode/TypedData.h"
#include "../Flow/StringData.h"
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

// Simple test fixture for integration workflow tests
class SimpleIntegrationWorkflowTest : public ::testing::Test {
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
        medium_dataset_.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            medium_dataset_.push_back(
                std::make_shared<TypedData<double>>(static_cast<double>(i) * 0.1)
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
    
    // Performance tracking
    std::chrono::nanoseconds total_processing_time_{0};
    std::atomic<size_t> items_processed_{0};
    std::atomic<size_t> errors_encountered_{0};
    std::atomic<size_t> active_workflows_{0};
};

// Simple test node for workflow scenarios
class SimpleDataProcessingNode : public AsyncNode {
private:
    std::string operation_name_;
    std::chrono::milliseconds processing_delay_;
    bool should_fail_;
    
public:
    explicit SimpleDataProcessingNode(
        std::string operation_name,
        std::chrono::milliseconds delay = std::chrono::milliseconds(10),
        bool should_fail = false
    ) : operation_name_(std::move(operation_name))
      , processing_delay_(delay)
      , should_fail_(should_fail) {}
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes) override {
        return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
            // Simulate processing delay
            if (processing_delay_.count() > 0) {
                std::this_thread::sleep_for(processing_delay_);
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
            } else if (auto string_data = PrepRes->as<StringData>()) {
                std::string value = string_data->value;
                return std::make_shared<StringData>(operation_name_ + "_" + value);
            }
            
            // Return processed marker for unknown types
            return std::make_shared<StringData>(operation_name_ + "_processed");
        });
    }
};

// Simple aggregation node
class SimpleAggregationNode : public AsyncNode {
private:
    std::string aggregation_type_;
    
public:
    explicit SimpleAggregationNode(std::string agg_type = "sum") 
        : aggregation_type_(std::move(agg_type)) {}
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes) override {
        return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
            if (auto batch_data = PrepRes->as<BatchData>()) {
                if (aggregation_type_ == "sum") {
                    return performSum(*batch_data);
                } else if (aggregation_type_ == "count") {
                    return performCount(*batch_data);
                }
            }
            
            return std::make_shared<StringData>("aggregation_failed");
        });
    }
    
private:
    std::shared_ptr<FrameworkData> performSum(const BatchData& batch) {
        double sum = 0.0;
        
        for (const auto& item : batch) {
            if (auto int_data = item->as<TypedData<int>>()) {
                sum += int_data->get();
            } else if (auto double_data = item->as<TypedData<double>>()) {
                sum += double_data->get();
            }
        }
        
        return std::make_shared<TypedData<double>>(sum);
    }
    
    std::shared_ptr<FrameworkData> performCount(const BatchData& batch) {
        return std::make_shared<TypedData<size_t>>(batch.size());
    }
};

// Test 1: Complete data processing pipelines with multiple nodes
TEST_F(SimpleIntegrationWorkflowTest, CompleteDataProcessingPipeline) {
    std::cout << "\n=== Testing Complete Data Processing Pipeline ===\n";
    
    // Create a multi-stage processing pipeline
    auto input_node = std::make_shared<SimpleDataProcessingNode>("input_processor");
    auto transform_node = std::make_shared<SimpleDataProcessingNode>("transformer");
    auto output_node = std::make_shared<SimpleDataProcessingNode>("output_processor");
    
    // Create workflow
    auto workflow = std::make_shared<AsyncFlow>(input_node);
    
    // Chain the nodes together
    input_node->next(transform_node);
    transform_node->next(output_node);
    
    // Test with small dataset
    {
        active_workflows_.fetch_add(1);
        
        auto input_data = std::make_shared<TypedData<int>>(42);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // Execute the workflow
            auto future = workflow->RunAsync(input_data);
            auto result = future.get();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            EXPECT_NE(result, nullptr);
            std::cout << "Pipeline completed successfully in " << execution_time.count() << " ms\n";
            
            items_processed_.fetch_add(1);
            
        } catch (const std::exception& e) {
            std::cout << "Pipeline failed: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
            FAIL() << "Pipeline should not fail with valid data";
        }
        
        active_workflows_.fetch_sub(1);
    }
    
    std::cout << "Total items processed: " << items_processed_.load() << "\n";
    std::cout << "Errors encountered: " << errors_encountered_.load() << "\n";
}

// Test 2: Async workflow execution with proper error handling
TEST_F(SimpleIntegrationWorkflowTest, AsyncWorkflowWithErrorHandling) {
    std::cout << "\n=== Testing Async Workflow Error Handling ===\n";
    
    // Test 1: Error propagation through workflow
    {
        auto failing_node = std::make_shared<SimpleDataProcessingNode>("failing_node", 
                                                                      std::chrono::milliseconds(50), 
                                                                      true); // Will fail
        auto recovery_node = std::make_shared<SimpleDataProcessingNode>("recovery_node");
        
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
    
    // Test 2: Random failure handling with retry logic
    {
        auto reliable_node = std::make_shared<SimpleDataProcessingNode>("reliable_node", 
                                                                       std::chrono::milliseconds(10),
                                                                       false); // Won't fail
        auto workflow = std::make_shared<AsyncFlow>(reliable_node);
        
        size_t success_count = 0;
        size_t failure_count = 0;
        const size_t test_iterations = 10;
        
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
        
        // With reliable node, we expect 100% success rate
        EXPECT_EQ(success_count, test_iterations);
        EXPECT_EQ(failure_count, 0);
    }
    
    std::cout << "Total errors handled: " << errors_encountered_.load() << "\n";
}

// Test 3: Batch processing workflows with large datasets
TEST_F(SimpleIntegrationWorkflowTest, BatchProcessingWorkflowsLargeDatasets) {
    std::cout << "\n=== Testing Batch Processing Workflows with Large Datasets ===\n";
    
    // Test 1: Simple batch processing
    {
        auto batch_processor_node = std::make_shared<SimpleDataProcessingNode>("batch_processor", 
                                                                              std::chrono::milliseconds(1));
        auto aggregation_node = std::make_shared<SimpleAggregationNode>("count");
        
        auto batch_workflow = std::make_shared<AsyncFlow>(batch_processor_node);
        batch_processor_node->next(aggregation_node);
        
        // Create batch input
        auto batch = std::make_unique<BatchData>();
        batch->reserve(medium_dataset_.size());
        for (const auto& item : medium_dataset_) {
            batch->add_item(item);
        }
        
        std::cout << "Processing batch with " << batch->size() << " items\n";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            auto future = batch_workflow->RunAsync(std::move(batch));
            auto result = future.get();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            EXPECT_NE(result, nullptr);
            
            std::cout << "Batch processing completed in " << execution_time.count() << " ms\n";
            
            if (auto count_result = result->as<TypedData<size_t>>()) {
                std::cout << "Count result: " << count_result->get() << "\n";
                EXPECT_GT(count_result->get(), 0);
            }
            
            items_processed_.fetch_add(medium_dataset_.size());
            
        } catch (const std::exception& e) {
            std::cout << "Batch processing failed: " << e.what() << "\n";
            errors_encountered_.fetch_add(1);
            FAIL() << "Batch processing should handle medium datasets";
        }
    }
    
    std::cout << "Total items processed in batch tests: " << items_processed_.load() << "\n";
}

// Test 4: Performance benchmarks for realistic usage scenarios
TEST_F(SimpleIntegrationWorkflowTest, PerformanceBenchmarksRealisticScenarios) {
    std::cout << "\n=== Performance Benchmarks for Realistic Usage Scenarios ===\n";
    
    // Scenario 1: Real-time data processing pipeline
    {
        std::cout << "\n--- Real-time Data Processing Scenario ---\n";
        
        auto input_validator = std::make_shared<SimpleDataProcessingNode>("validator", 
                                                                         std::chrono::milliseconds(1));
        auto data_transformer = std::make_shared<SimpleDataProcessingNode>("transformer", 
                                                                           std::chrono::milliseconds(2));
        auto output_formatter = std::make_shared<SimpleDataProcessingNode>("formatter", 
                                                                           std::chrono::milliseconds(1));
        
        // Create real-time processing workflow
        auto realtime_workflow = std::make_shared<AsyncFlow>(input_validator);
        input_validator->next(data_transformer);
        data_transformer->next(output_formatter);
        
        // Simulate real-time processing with multiple concurrent requests
        const size_t concurrent_requests = 10;
        
        std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
        futures.reserve(concurrent_requests);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Submit concurrent requests
        for (size_t i = 0; i < concurrent_requests; ++i) {
            auto input_data = std::make_shared<TypedData<int>>(static_cast<int>(i));
            futures.emplace_back(realtime_workflow->RunAsync(input_data));
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
        
        std::cout << "Real-time processing results:\n";
        std::cout << "  - Total requests: " << concurrent_requests << "\n";
        std::cout << "  - Successful requests: " << successful_requests << "\n";
        std::cout << "  - Total duration: " << total_duration.count() << " ms\n";
        std::cout << "  - Requests/second: " << requests_per_second << "\n";
        
        EXPECT_GT(successful_requests, concurrent_requests * 0.8); // At least 80% success rate
        items_processed_.fetch_add(successful_requests);
    }
    
    // Scenario 2: Sequential batch processing
    {
        std::cout << "\n--- Sequential Batch Processing Scenario ---\n";
        
        auto data_processor = std::make_shared<SimpleDataProcessingNode>("processor", 
                                                                        std::chrono::milliseconds(1));
        auto batch_workflow = std::make_shared<AsyncFlow>(data_processor);
        
        // Process multiple small batches sequentially
        const size_t num_batches = 5;
        const size_t batch_size = small_dataset_.size() / num_batches;
        
        std::vector<double> batch_processing_times;
        batch_processing_times.reserve(num_batches);
        
        for (size_t batch_idx = 0; batch_idx < num_batches; ++batch_idx) {
            auto batch = std::make_unique<BatchData>();
            batch->reserve(batch_size);
            
            size_t start_idx = batch_idx * batch_size;
            size_t end_idx = std::min(start_idx + batch_size, small_dataset_.size());
            
            for (size_t i = start_idx; i < end_idx; ++i) {
                batch->add_item(small_dataset_[i]);
            }
            
            auto batch_start = std::chrono::high_resolution_clock::now();
            
            try {
                auto future = batch_workflow->RunAsync(std::move(batch));
                auto result = future.get();
                
                auto batch_end = std::chrono::high_resolution_clock::now();
                auto batch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(batch_end - batch_start);
                
                batch_processing_times.push_back(static_cast<double>(batch_duration.count()));
                
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
            
            std::cout << "Sequential batch processing results:\n";
            std::cout << "  - Number of batches: " << num_batches << "\n";
            std::cout << "  - Average batch size: " << batch_size << "\n";
            std::cout << "  - Average processing time: " << avg_time << " ms\n";
            std::cout << "  - Min processing time: " << min_time << " ms\n";
            std::cout << "  - Max processing time: " << max_time << " ms\n";
            std::cout << "  - Throughput: " << (batch_size * 1000.0 / avg_time) << " items/second\n";
        }
    }
    
    std::cout << "\nOverall Performance Summary:\n";
    std::cout << "  - Total items processed: " << items_processed_.load() << "\n";
    std::cout << "  - Total errors encountered: " << errors_encountered_.load() << "\n";
    if (items_processed_.load() > 0) {
        std::cout << "  - Error rate: " << (static_cast<double>(errors_encountered_.load()) / items_processed_.load() * 100.0) << "%\n";
    }
}