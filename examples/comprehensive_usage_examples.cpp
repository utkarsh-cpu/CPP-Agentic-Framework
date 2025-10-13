/**
 * Comprehensive Usage Examples for C++ Agentic Framework
 * 
 * This file demonstrates common usage patterns, best practices,
 * and real-world scenarios using the framework.
 */

#include "BaseNode/BaseNode.h"
#include "Node/Node.h"
#include "AsyncNode/AsyncNode.h"
#include "BatchNode/BatchNode.h"
#include "AsyncBatchNode/AsyncBatchNode.h"
#include "AsyncParallelBatchNode/AsyncParallelBatchNode.h"
#include "Flow/Flow.h"
#include "AsyncFlow/AsyncFlow.h"
#include "BatchFlow/BatchFlow.h"
#include "AsyncBatchFlow/AsyncBatchFlow.h"
#include "AsyncParallelBatchFlow/AsyncParallelBatchFlow.h"
#include "BaseNode/TypedData.h"
#include "BaseNode/Result.h"
#include "BaseNode/FrameworkError.h"
#include "BaseNode/CancellationToken.h"
#include "Flow/StringData.h"
#include "BatchNode/VectorData.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

// =============================================================================
// Example 1: Basic Data Processing Pipeline
// =============================================================================

class DataValidationNode : public Node {
public:
    DataValidationNode() : Node(3, 1000) {  // 3 retries, 1 second wait
        // Set validation rules as parameters
        std::map<std::string, std::shared_ptr<FrameworkData>> params;
        params["min_length"] = std::make_shared<TypedData<int>>(5);
        params["max_length"] = std::make_shared<TypedData<int>>(100);
        setParam(std::move(params));
    }
    
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* input = prep_res->as<StringData>()) {
            // Get validation parameters
            auto min_len_param = getParam("min_length");
            auto max_len_param = getParam("max_length");
            
            int min_length = 0, max_length = 1000;
            if (auto* min_data = min_len_param->as<TypedData<int>>()) {
                min_length = min_data->get();
            }
            if (auto* max_data = max_len_param->as<TypedData<int>>()) {
                max_length = max_data->get();
            }
            
            // Validate input
            if (input->value.length() < static_cast<size_t>(min_length)) {
                throw FrameworkError::invalid_argument(
                    "Input too short (minimum " + std::to_string(min_length) + " characters)",
                    "DataValidationNode::Exec"
                );
            }
            
            if (input->value.length() > static_cast<size_t>(max_length)) {
                throw FrameworkError::invalid_argument(
                    "Input too long (maximum " + std::to_string(max_length) + " characters)",
                    "DataValidationNode::Exec"
                );
            }
            
            // Return validated data with metadata
            return std::make_shared<StringData>("VALIDATED:" + input->value);
        }
        
        throw FrameworkError::invalid_argument("Expected StringData input", "DataValidationNode::Exec");
    }
    
    std::shared_ptr<FrameworkData> exec_fallback(
        const std::shared_ptr<FrameworkData>& prep_res,
        std::exception_ptr eptr) const override {
        
        try {
            std::rethrow_exception(eptr);
        } catch (const FrameworkError& e) {
            std::cerr << "Validation failed: " << e.what() << std::endl;
            // Return sanitized version
            if (auto* input = prep_res->as<StringData>()) {
                std::string sanitized = input->value.substr(0, 50);  // Truncate
                return std::make_shared<StringData>("SANITIZED:" + sanitized);
            }
        }
        
        return std::make_shared<StringData>("FALLBACK:invalid_input");
    }
};

class DataTransformationNode : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [prep_res]() {
            // Simulate CPU-intensive transformation
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (auto* input = prep_res->as<StringData>()) {
                std::string transformed = input->value;
                
                // Apply transformations
                std::transform(transformed.begin(), transformed.end(), 
                             transformed.begin(), ::toupper);
                
                // Add processing timestamp
                auto now = std::chrono::system_clock::now();
                auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch()
                ).count();
                
                transformed += "_PROCESSED_AT_" + std::to_string(timestamp);
                
                return std::static_pointer_cast<FrameworkData>(
                    std::make_shared<StringData>(transformed)
                );
            }
            
            throw FrameworkError::invalid_argument("Expected StringData", "DataTransformationNode::ExecAsync");
        });
    }
};

class DataPersistenceNode : public Node {
private:
    std::string output_directory_;
    
public:
    explicit DataPersistenceNode(std::string output_dir = "./output/") 
        : output_directory_(std::move(output_dir)) {
        
        // Create output directory if it doesn't exist
        std::filesystem::create_directories(output_directory_);
    }
    
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* input = prep_res->as<StringData>()) {
            // Generate unique filename
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ).count();
            
            std::string filename = output_directory_ + "processed_" + 
                                 std::to_string(timestamp) + ".txt";
            
            // Write to file
            std::ofstream file(filename);
            if (!file) {
                throw FrameworkError::runtime_error(
                    "Failed to create output file: " + filename,
                    "DataPersistenceNode::Exec"
                );
            }
            
            file << input->value << std::endl;
            file.close();
            
            // Return success result with filename
            return std::make_shared<StringData>("SAVED:" + filename);
        }
        
        throw FrameworkError::invalid_argument("Expected StringData", "DataPersistenceNode::Exec");
    }
};

void example_basic_pipeline() {
    std::cout << "\n=== Example 1: Basic Data Processing Pipeline ===" << std::endl;
    
    try {
        // Create processing nodes
        auto validator = std::make_shared<DataValidationNode>();
        auto transformer = std::make_shared<DataTransformationNode>();
        auto persister = std::make_shared<DataPersistenceNode>();
        
        // Chain nodes together
        validator->next(transformer);
        transformer->next(persister);
        
        // Create flow
        auto flow = std::make_shared<Flow>(validator);
        
        // Process different types of input
        std::vector<std::string> test_inputs = {
            "Hello World",
            "This is a valid input string",
            "Short",  // Will trigger fallback
            "This is a very long string that exceeds the maximum length limit and should be handled gracefully"
        };
        
        for (const auto& input_text : test_inputs) {
            std::cout << "\nProcessing: \"" << input_text << "\"" << std::endl;
            
            auto input = std::make_shared<StringData>(input_text);
            auto result = flow->orch(input);
            
            if (auto* output = result->as<StringData>()) {
                std::cout << "Result: " << output->value << std::endl;
            }
        }
        
    } catch (const FrameworkError& e) {
        std::cerr << "Framework error: " << e.what() << " (Context: " << e.context() << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Standard error: " << e.what() << std::endl;
    }
}

// =============================================================================
// Example 2: Batch Processing with Performance Optimization
// =============================================================================

class BatchDataProcessor : public BatchNode {
private:
    std::atomic<size_t> processed_count_{0};
    std::atomic<size_t> error_count_{0};
    
public:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw FrameworkError::invalid_argument("Expected VectorData for batch processing", "BatchDataProcessor::exec");
        }
        
        auto result = std::make_shared<VectorData>();
        result->data.reserve(batch->data.size());  // Optimize memory allocation
        
        // Process each item with error handling
        for (const auto& item : batch->data) {
            try {
                auto processed_item = process_single_item(item);
                result->data.push_back(processed_item);
                processed_count_.fetch_add(1, std::memory_order_relaxed);
                
            } catch (const std::exception& e) {
                error_count_.fetch_add(1, std::memory_order_relaxed);
                std::cerr << "Error processing batch item: " << e.what() << std::endl;
                
                // Add error marker instead of failing entire batch
                result->data.push_back(
                    std::make_shared<StringData>("ERROR: " + std::string(e.what()))
                );
            }
        }
        
        return result;
    }
    
    // Get processing statistics
    std::pair<size_t, size_t> get_stats() const {
        return {
            processed_count_.load(std::memory_order_relaxed),
            error_count_.load(std::memory_order_relaxed)
        };
    }
    
private:
    std::shared_ptr<FrameworkData> process_single_item(std::shared_ptr<FrameworkData> item) {
        if (auto* str_item = item->as<StringData>()) {
            // Simulate processing with random delay
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> delay_dist(10, 100);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
            
            // Simulate occasional errors
            std::uniform_int_distribution<> error_dist(1, 20);
            if (error_dist(gen) == 1) {  // 5% error rate
                throw std::runtime_error("Simulated processing error");
            }
            
            return std::make_shared<StringData>("BATCH_PROCESSED: " + str_item->value);
        }
        
        return item;
    }
};

void example_batch_processing() {
    std::cout << "\n=== Example 2: Batch Processing ===" << std::endl;
    
    try {
        auto batch_processor = std::make_shared<BatchDataProcessor>();
        
        // Create large batch of data
        auto batch = std::make_shared<VectorData>();
        const size_t batch_size = 100;
        batch->data.reserve(batch_size);
        
        for (size_t i = 0; i < batch_size; ++i) {
            batch->data.emplace_back(
                std::make_shared<StringData>("Item_" + std::to_string(i))
            );
        }
        
        std::cout << "Processing batch of " << batch_size << " items..." << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        auto result = batch_processor->Execute(batch);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );
        
        // Display results
        if (auto* result_batch = result->as<VectorData>()) {
            std::cout << "Processed " << result_batch->data.size() << " items in " 
                      << duration.count() << " ms" << std::endl;
            
            auto [processed, errors] = batch_processor->get_stats();
            std::cout << "Success: " << processed << ", Errors: " << errors << std::endl;
            
            // Show first few results
            std::cout << "Sample results:" << std::endl;
            for (size_t i = 0; i < std::min(size_t(5), result_batch->data.size()); ++i) {
                if (auto* item = result_batch->data[i]->as<StringData>()) {
                    std::cout << "  " << item->value << std::endl;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Batch processing error: " << e.what() << std::endl;
    }
}

// =============================================================================
// Example 3: Async Processing with Cancellation and Timeouts
// =============================================================================

class LongRunningAsyncNode : public AsyncNode {
private:
    std::string operation_name_;
    
public:
    explicit LongRunningAsyncNode(std::string name) : operation_name_(std::move(name)) {}
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [this, prep_res]() {
            return simulate_long_operation(prep_res);
        });
    }
    
    std::future<std::shared_ptr<FrameworkData>> RunAsyncWithCancellation(
        std::shared_ptr<FrameworkData> shared,
        const CancellationToken& token) override {
        
        return std::async(std::launch::async, [this, shared, &token]() {
            return simulate_cancellable_operation(shared, token);
        });
    }
    
private:
    std::shared_ptr<FrameworkData> simulate_long_operation(std::shared_ptr<FrameworkData> data) {
        std::cout << "Starting " << operation_name_ << "..." << std::endl;
        
        // Simulate work that takes 3-7 seconds
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> duration_dist(3000, 7000);
        
        int duration_ms = duration_dist(gen);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        
        if (auto* input = data->as<StringData>()) {
            return std::make_shared<StringData>(
                operation_name_ + "_COMPLETED_IN_" + std::to_string(duration_ms) + "ms: " + input->value
            );
        }
        
        return data;
    }
    
    std::shared_ptr<FrameworkData> simulate_cancellable_operation(
        std::shared_ptr<FrameworkData> data,
        const CancellationToken& token) {
        
        std::cout << "Starting cancellable " << operation_name_ << "..." << std::endl;
        
        // Simulate work in small chunks, checking for cancellation
        const int total_steps = 50;
        for (int step = 0; step < total_steps; ++step) {
            // Check for cancellation
            if (token.is_cancelled()) {
                std::cout << operation_name_ << " cancelled at step " << step << "/" << total_steps << std::endl;
                
                if (auto* input = data->as<StringData>()) {
                    return std::make_shared<StringData>(
                        operation_name_ + "_CANCELLED_AT_STEP_" + std::to_string(step) + ": " + input->value
                    );
                }
                return data;
            }
            
            // Simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (step % 10 == 0) {
                std::cout << operation_name_ << " progress: " << step << "/" << total_steps << std::endl;
            }
        }
        
        if (auto* input = data->as<StringData>()) {
            return std::make_shared<StringData>(
                operation_name_ + "_COMPLETED_SUCCESSFULLY: " + input->value
            );
        }
        
        return data;
    }
};

void example_async_with_cancellation() {
    std::cout << "\n=== Example 3: Async Processing with Cancellation ===" << std::endl;
    
    try {
        auto async_node = std::make_shared<LongRunningAsyncNode>("DataAnalysis");
        auto input = std::make_shared<StringData>("sample_data");
        
        // Example 3a: Normal async execution with timeout
        std::cout << "\n--- 3a: Async with timeout ---" << std::endl;
        
        auto future = async_node->RunAsyncWithTimeout(input, std::chrono::seconds(5));
        
        try {
            auto result = future.get();
            if (auto* output = result->as<StringData>()) {
                std::cout << "Result: " << output->value << std::endl;
            }
        } catch (const std::runtime_error& e) {
            std::cout << "Operation timed out: " << e.what() << std::endl;
        }
        
        // Example 3b: Cancellable execution
        std::cout << "\n--- 3b: Cancellable execution ---" << std::endl;
        
        CancellationToken token;
        auto cancel_future = async_node->RunAsyncWithCancellation(input, token);
        
        // Cancel after 2 seconds
        std::thread canceller([&token]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "Requesting cancellation..." << std::endl;
            token.cancel();
        });
        
        try {
            auto result = cancel_future.get();
            if (auto* output = result->as<StringData>()) {
                std::cout << "Result: " << output->value << std::endl;
            }
        } catch (const std::runtime_error& e) {
            std::cout << "Operation was cancelled: " << e.what() << std::endl;
        }
        
        canceller.join();
        
    } catch (const std::exception& e) {
        std::cerr << "Async processing error: " << e.what() << std::endl;
    }
}

// =============================================================================
// Example 4: Error Handling with Result Types
// =============================================================================

using namespace framework;

Result<std::shared_ptr<FrameworkData>> safe_data_processing(
    std::shared_ptr<FrameworkData> input) {
    
    if (!input) {
        return FrameworkError::invalid_argument("Input cannot be null", "safe_data_processing");
    }
    
    try {
        if (auto* str_data = input->as<StringData>()) {
            // Simulate processing that might fail
            if (str_data->value.empty()) {
                return FrameworkError::invalid_argument("Input string cannot be empty", "safe_data_processing");
            }
            
            if (str_data->value.find("error") != std::string::npos) {
                return FrameworkError::runtime_error("Processing failed due to error keyword", "safe_data_processing");
            }
            
            // Successful processing
            auto result = std::make_shared<StringData>("SAFELY_PROCESSED: " + str_data->value);
            return std::static_pointer_cast<FrameworkData>(result);
        }
        
        return FrameworkError::invalid_argument("Unsupported data type", "safe_data_processing");
        
    } catch (const std::exception& e) {
        return FrameworkError::runtime_error(e.what(), "safe_data_processing");
    }
}

Result<std::string> extract_processed_value(std::shared_ptr<FrameworkData> data) {
    if (!data) {
        return FrameworkError::invalid_argument("Data cannot be null", "extract_processed_value");
    }
    
    if (auto* str_data = data->as<StringData>()) {
        return str_data->value;
    }
    
    return FrameworkError::invalid_argument("Expected StringData", "extract_processed_value");
}

void example_result_types() {
    std::cout << "\n=== Example 4: Error Handling with Result Types ===" << std::endl;
    
    std::vector<std::string> test_inputs = {
        "valid input",
        "",  // Empty string - will cause error
        "input with error keyword",  // Will cause processing error
        "another valid input"
    };
    
    for (const auto& input_text : test_inputs) {
        std::cout << "\nProcessing: \"" << input_text << "\"" << std::endl;
        
        auto input = std::make_shared<StringData>(input_text);
        
        // Chain operations with automatic error propagation
        auto final_result = safe_data_processing(input)
            .flat_map([](auto data) { 
                return safe_data_processing(data);  // Chain another operation
            })
            .flat_map([](auto data) { 
                return extract_processed_value(data); 
            })
            .map([](const std::string& value) {
                std::cout << "  Success: " << value << std::endl;
                return value;
            })
            .map_error([](const FrameworkError& e) {
                std::cout << "  Error: " << e.what() << " (Context: " << e.context() << ")" << std::endl;
                return e;
            });
        
        // Get final result or default value
        std::string result = final_result.value_or("PROCESSING_FAILED");
        std::cout << "  Final result: " << result << std::endl;
    }
}

// =============================================================================
// Example 5: Type-Safe Data with Validation
// =============================================================================

class TypeSafeProcessingNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        // Process different typed data safely
        
        if (auto* int_data = prep_res->as<TypedData<int>>()) {
            int value = int_data->get();
            
            // Create result with validation
            auto result = std::make_unique<TypedData<int>>(
                value * 2,
                std::vector<Constraint<int>>{
                    Constraints<int>::range(0, 1000)
                }
            );
            
            return std::static_pointer_cast<FrameworkData>(std::move(result));
        }
        
        if (auto* string_data = prep_res->as<TypedData<std::string>>()) {
            std::string value = string_data->get();
            
            // Create result with validation
            auto result = std::make_unique<TypedData<std::string>>(
                "PROCESSED: " + value,
                std::vector<Constraint<std::string>>{
                    Constraints<std::string>::not_empty(),
                    Constraints<std::string>::custom(
                        [](const std::string& s) { return s.length() <= 200; },
                        "String must be <= 200 characters"
                    )
                }
            );
            
            return std::static_pointer_cast<FrameworkData>(std::move(result));
        }
        
        throw FrameworkError::invalid_argument("Unsupported data type", "TypeSafeProcessingNode::Exec");
    }
};

void example_type_safe_processing() {
    std::cout << "\n=== Example 5: Type-Safe Data Processing ===" << std::endl;
    
    try {
        auto processor = std::make_shared<TypeSafeProcessingNode>();
        
        // Test with integer data
        std::cout << "\n--- Processing integer data ---" << std::endl;
        
        auto int_input = std::make_unique<TypedData<int>>(
            42,
            std::vector<Constraint<int>>{
                Constraints<int>::range(1, 100)
            }
        );
        
        auto int_result = processor->Execute(std::move(int_input));
        
        if (auto* result_data = int_result->as<TypedData<int>>()) {
            std::cout << "Integer result: " << result_data->get() << std::endl;
            std::cout << "Is valid: " << (result_data->is_valid() ? "Yes" : "No") << std::endl;
        }
        
        // Test with string data
        std::cout << "\n--- Processing string data ---" << std::endl;
        
        auto string_input = std::make_unique<TypedData<std::string>>(
            "Hello World",
            std::vector<Constraint<std::string>>{
                Constraints<std::string>::not_empty()
            }
        );
        
        auto string_result = processor->Execute(std::move(string_input));
        
        if (auto* result_data = string_result->as<TypedData<std::string>>()) {
            std::cout << "String result: " << result_data->get() << std::endl;
            std::cout << "Is valid: " << (result_data->is_valid() ? "Yes" : "No") << std::endl;
            
            // Check validation errors
            auto errors = result_data->get_validation_errors();
            if (!errors.empty()) {
                std::cout << "Validation errors:" << std::endl;
                for (const auto& error : errors) {
                    std::cout << "  - " << error << std::endl;
                }
            }
        }
        
        // Test constraint violation
        std::cout << "\n--- Testing constraint violation ---" << std::endl;
        
        try {
            auto invalid_input = std::make_unique<TypedData<int>>(
                500,  // This will violate the range constraint after processing (500 * 2 = 1000)
                std::vector<Constraint<int>>{
                    Constraints<int>::range(1, 100)
                }
            );
            
            auto invalid_result = processor->Execute(std::move(invalid_input));
            
        } catch (const std::invalid_argument& e) {
            std::cout << "Constraint violation caught: " << e.what() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Type-safe processing error: " << e.what() << std::endl;
    }
}

// =============================================================================
// Example 6: Complex Flow with Conditional Routing
// =============================================================================

class ConditionalRoutingNode : public Node {
public:
    std::shared_ptr<FrameworkData> Post(
        std::shared_ptr<FrameworkData> shared,
        std::shared_ptr<FrameworkData> prep_res,
        std::shared_ptr<FrameworkData> exec_res) override {
        
        // Determine routing based on processing result
        if (auto* result = exec_res->as<StringData>()) {
            if (result->value.find("ERROR") != std::string::npos) {
                // Route to error handler
                return std::make_shared<StringData>("ROUTE:error");
            } else if (result->value.find("PRIORITY") != std::string::npos) {
                // Route to priority processor
                return std::make_shared<StringData>("ROUTE:priority");
            } else {
                // Route to normal processor
                return std::make_shared<StringData>("ROUTE:normal");
            }
        }
        
        return exec_res;
    }
    
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* input = prep_res->as<StringData>()) {
            // Simulate classification logic
            std::string classification;
            
            if (input->value.find("urgent") != std::string::npos) {
                classification = "PRIORITY: ";
            } else if (input->value.find("invalid") != std::string::npos) {
                classification = "ERROR: ";
            } else {
                classification = "NORMAL: ";
            }
            
            return std::make_shared<StringData>(classification + input->value);
        }
        
        return prep_res;
    }
};

class ErrorHandlerNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* input = prep_res->as<StringData>()) {
            std::cout << "  [ERROR HANDLER] Processing: " << input->value << std::endl;
            return std::make_shared<StringData>("ERROR_HANDLED: " + input->value);
        }
        return prep_res;
    }
};

class PriorityProcessorNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* input = prep_res->as<StringData>()) {
            std::cout << "  [PRIORITY PROCESSOR] Processing: " << input->value << std::endl;
            return std::make_shared<StringData>("PRIORITY_PROCESSED: " + input->value);
        }
        return prep_res;
    }
};

class NormalProcessorNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> prep_res) override {
        if (auto* input = prep_res->as<StringData>()) {
            std::cout << "  [NORMAL PROCESSOR] Processing: " << input->value << std::endl;
            return std::make_shared<StringData>("NORMAL_PROCESSED: " + input->value);
        }
        return prep_res;
    }
};

void example_conditional_routing() {
    std::cout << "\n=== Example 6: Complex Flow with Conditional Routing ===" << std::endl;
    
    try {
        // Create nodes
        auto router = std::make_shared<ConditionalRoutingNode>();
        auto error_handler = std::make_shared<ErrorHandlerNode>();
        auto priority_processor = std::make_shared<PriorityProcessorNode>();
        auto normal_processor = std::make_shared<NormalProcessorNode>();
        
        // Set up conditional routing
        router->next(error_handler, "error");
        router->next(priority_processor, "priority");
        router->next(normal_processor, "normal");
        
        // Create flow
        auto flow = std::make_shared<Flow>(router);
        
        // Test different types of input
        std::vector<std::string> test_inputs = {
            "normal processing request",
            "urgent priority task",
            "invalid data input",
            "another normal request"
        };
        
        for (const auto& input_text : test_inputs) {
            std::cout << "\nProcessing: \"" << input_text << "\"" << std::endl;
            
            auto input = std::make_shared<StringData>(input_text);
            auto result = flow->orch(input);
            
            if (auto* output = result->as<StringData>()) {
                std::cout << "  Final result: " << output->value << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Conditional routing error: " << e.what() << std::endl;
    }
}

// =============================================================================
// Example 7: AsyncFlow - Asynchronous Workflow Management
// =============================================================================

#include "AsyncFlow/AsyncFlow.h"

class AsyncDataLoaderNode : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [prep_res]() {
            // Simulate loading data from external source
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            if (auto* input = prep_res->as<StringData>()) {
                return std::static_pointer_cast<FrameworkData>(
                    std::make_shared<StringData>("LOADED_DATA: " + input->value)
                );
            }
            throw std::invalid_argument("Expected StringData");
        });
    }
};

class AsyncDataProcessorNode : public AsyncNode {
public:
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(
        std::shared_ptr<FrameworkData> prep_res) override {
        
        return std::async(std::launch::async, [prep_res]() {
            // Simulate CPU-intensive processing
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
            if (auto* input = prep_res->as<StringData>()) {
                return std::static_pointer_cast<FrameworkData>(
                    std::make_shared<StringData>("ASYNC_PROCESSED: " + input->value)
                );
            }
            throw std::invalid_argument("Expected StringData");
        });
    }
};

void example_async_flow() {
    std::cout << "\n=== Example 7: AsyncFlow - Asynchronous Workflow Management ===" << std::endl;
    
    try {
        // Create async nodes
        auto loader = std::make_shared<AsyncDataLoaderNode>();
        auto processor = std::make_shared<AsyncDataProcessorNode>();
        auto validator = std::make_shared<AsyncDataLoaderNode>();  // Reuse for validation
        
        // Set up async flow
        loader->next(processor, "loaded");
        processor->next(validator, "processed");
        
        // Create async flow
        auto async_flow = std::make_shared<AsyncFlow>(loader);
        
        std::vector<std::string> test_inputs = {
            "dataset_1",
            "dataset_2", 
            "dataset_3"
        };
        
        // Process multiple inputs concurrently
        std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
        
        for (const auto& input_text : test_inputs) {
            auto input = std::make_shared<StringData>(input_text);
            futures.emplace_back(async_flow->RunAsync(input));
        }
        
        // Collect results as they complete
        for (size_t i = 0; i < futures.size(); ++i) {
            auto result = futures[i].get();
            if (auto* output = result->as<StringData>()) {
                std::cout << "Async flow result " << (i + 1) << ": " << output->value << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "AsyncFlow error: " << e.what() << std::endl;
    }
}

// =============================================================================
// Example 8: AsyncBatchFlow - Asynchronous Batch Processing
// =============================================================================

#include "AsyncBatchFlow/AsyncBatchFlow.h"

class AsyncBatchLoaderNode : public AsyncBatchNode {
private:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData for async batch loading");
        }
        
        // Simulate async loading of batch items
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        auto result = std::make_shared<VectorData>();
        result->data.reserve(batch->data.size());
        
        for (const auto& item : batch->data) {
            if (auto* str_item = item->as<StringData>()) {
                result->data.push_back(
                    std::make_shared<StringData>("ASYNC_LOADED: " + str_item->value)
                );
            }
        }
        
        return result;
    }
};

class AsyncBatchProcessorNode : public AsyncBatchNode {
private:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData for async batch processing");
        }
        
        // Simulate intensive batch processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100 * batch->data.size()));
        
        auto result = std::make_shared<VectorData>();
        result->data.reserve(batch->data.size());
        
        for (const auto& item : batch->data) {
            if (auto* str_item = item->as<StringData>()) {
                result->data.push_back(
                    std::make_shared<StringData>("ASYNC_BATCH_PROCESSED: " + str_item->value)
                );
            }
        }
        
        return result;
    }
};

void example_async_batch_flow() {
    std::cout << "\n=== Example 8: AsyncBatchFlow - Asynchronous Batch Processing ===" << std::endl;
    
    try {
        // Create async batch nodes
        auto batch_loader = std::make_shared<AsyncBatchLoaderNode>();
        auto batch_processor = std::make_shared<AsyncBatchProcessorNode>();
        
        // Set up async batch flow
        batch_loader->next(batch_processor, "loaded");
        
        // Create async batch flow
        auto async_batch_flow = std::make_shared<AsyncBatchFlow>(batch_loader);
        
        // Create batch data
        auto batch = std::make_shared<VectorData>();
        const size_t batch_size = 50;
        
        for (size_t i = 0; i < batch_size; ++i) {
            batch->data.push_back(
                std::make_shared<StringData>("async_batch_item_" + std::to_string(i))
            );
        }
        
        std::cout << "Processing batch of " << batch_size << " items asynchronously..." << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Process batch asynchronously
        auto future = async_batch_flow->RunAsync(batch);
        
        // Show progress while processing
        std::cout << "Batch processing in background..." << std::endl;
        
        auto result = future.get();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );
        
        if (auto* result_batch = result->as<VectorData>()) {
            std::cout << "Async batch processing completed!" << std::endl;
            std::cout << "Processed " << result_batch->data.size() 
                      << " items in " << duration.count() << " ms" << std::endl;
            
            // Show sample results
            std::cout << "Sample results:" << std::endl;
            for (size_t i = 0; i < std::min(size_t(3), result_batch->data.size()); ++i) {
                if (auto* item = result_batch->data[i]->as<StringData>()) {
                    std::cout << "  " << item->value << std::endl;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "AsyncBatchFlow error: " << e.what() << std::endl;
    }
}

// =============================================================================
// Example 9: AsyncParallelBatchFlow - Maximum Performance Processing
// =============================================================================

#include "AsyncParallelBatchFlow/AsyncParallelBatchFlow.h"

class ParallelBatchSplitterNode : public AsyncParallelBatchNode {
private:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData for parallel batch splitting");
        }
        
        std::cout << "  [SPLITTER] Preparing " << batch->data.size() << " items for parallel processing" << std::endl;
        
        // In a real implementation, this might split the batch into optimal chunks
        // For this example, we'll just pass through with metadata
        auto result = std::make_shared<VectorData>();
        result->data.reserve(batch->data.size());
        
        for (const auto& item : batch->data) {
            if (auto* str_item = item->as<StringData>()) {
                result->data.push_back(
                    std::make_shared<StringData>("SPLIT_FOR_PARALLEL: " + str_item->value)
                );
            }
        }
        
        return result;
    }
};

class ParallelBatchProcessorNode : public AsyncParallelBatchNode {
private:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override {
        auto* batch = items->as<VectorData>();
        if (!batch) {
            throw std::invalid_argument("Expected VectorData for parallel batch processing");
        }
        
        std::cout << "  [PARALLEL PROCESSOR] Processing " << batch->data.size() 
                  << " items in parallel" << std::endl;
        
        // Process items in parallel using futures
        std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
        futures.reserve(batch->data.size());
        
        // Launch parallel processing tasks
        for (const auto& item : batch->data) {
            futures.emplace_back(
                std::async(std::launch::async, [item]() -> std::shared_ptr<FrameworkData> {
                    if (auto* str_item = item->as<StringData>()) {
                        // Simulate CPU-intensive work
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        
                        return std::make_shared<StringData>(
                            "PARALLEL_PROCESSED: " + str_item->value
                        );
                    }
                    return item;
                })
            );
        }
        
        // Collect results
        auto result = std::make_shared<VectorData>();
        result->data.reserve(futures.size());
        
        for (auto& future : futures) {
            result->data.push_back(future.get());
        }
        
        return result;
    }
};

void example_async_parallel_batch_flow() {
    std::cout << "\n=== Example 9: AsyncParallelBatchFlow - Maximum Performance Processing ===" << std::endl;
    
    try {
        // Create parallel batch processing nodes
        auto splitter = std::make_shared<ParallelBatchSplitterNode>();
        auto parallel_processor = std::make_shared<ParallelBatchProcessorNode>();
        
        // Set up parallel batch flow
        splitter->next(parallel_processor, "split");
        
        // Create parallel batch flow
        auto parallel_flow = std::make_shared<AsyncParallelBatchFlow>(splitter);
        
        // Create large batch for parallel processing
        auto large_batch = std::make_shared<VectorData>();
        const size_t batch_size = 100;  // Reduced for demo purposes
        
        for (size_t i = 0; i < batch_size; ++i) {
            large_batch->data.push_back(
                std::make_shared<StringData>("parallel_item_" + std::to_string(i))
            );
        }
        
        std::cout << "Processing batch of " << batch_size 
                  << " items with maximum parallelism..." << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Process with maximum parallelism
        auto future = parallel_flow->RunAsync(large_batch);
        
        std::cout << "Parallel batch processing in progress..." << std::endl;
        
        auto result = future.get();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );
        
        if (auto* result_batch = result->as<VectorData>()) {
            std::cout << "Parallel batch processing completed!" << std::endl;
            std::cout << "Processed " << result_batch->data.size() 
                      << " items in " << duration.count() << " ms using parallel execution" << std::endl;
            
            // Calculate theoretical speedup
            double sequential_time = batch_size * 50;  // 50ms per item
            double speedup = sequential_time / duration.count();
            std::cout << "Theoretical speedup: " << std::fixed << std::setprecision(2) 
                      << speedup << "x" << std::endl;
            
            // Show sample results
            std::cout << "Sample results:" << std::endl;
            for (size_t i = 0; i < std::min(size_t(3), result_batch->data.size()); ++i) {
                if (auto* item = result_batch->data[i]->as<StringData>()) {
                    std::cout << "  " << item->value << std::endl;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "AsyncParallelBatchFlow error: " << e.what() << std::endl;
    }
}

// =============================================================================
// Main Function - Run All Examples
// =============================================================================

int main() {
    std::cout << "C++ Agentic Framework - Comprehensive Usage Examples" << std::endl;
    std::cout << "=====================================================" << std::endl;
    
    try {
        example_basic_pipeline();
        example_batch_processing();
        example_async_with_cancellation();
        example_result_types();
        example_type_safe_processing();
        example_conditional_routing();
        example_async_flow();
        example_async_batch_flow();
        example_async_parallel_batch_flow();
        
        std::cout << "\n=== All Examples Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nFatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}