#include "../AsyncNode/AsyncNode.h"
#include "../BaseNode/AsyncTimeout.h"
#include "../BaseNode/CancellationToken.h"
#include "../BaseNode/TimeoutPolicy.h"
#include "../Flow/StringData.h"
#include <iostream>
#include <chrono>
#include <thread>

/**
 * @brief Example AsyncNode that simulates long-running work
 */
class LongRunningAsyncNode : public AsyncNode {
private:
    std::chrono::milliseconds work_duration_;
    bool should_respect_cancellation_;

public:
    explicit LongRunningAsyncNode(
        std::chrono::milliseconds work_duration = std::chrono::seconds(10),
        bool respect_cancellation = true
    ) : work_duration_(work_duration), should_respect_cancellation_(respect_cancellation) {}

    std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes) override {
        return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
            std::cout << "Starting long-running work (" << work_duration_.count() << "ms)..." << std::endl;
            
            // Simulate work in small chunks to allow for cancellation checking
            auto chunk_duration = std::chrono::milliseconds(100);
            auto chunks = work_duration_ / chunk_duration;
            
            for (int i = 0; i < static_cast<int>(chunks); ++i) {
                std::this_thread::sleep_for(chunk_duration);
                
                // In a real implementation, you would check for cancellation here
                // For this example, we'll just print progress
                if (i % 10 == 0) {
                    std::cout << "Progress: " << (i * 100 / static_cast<int>(chunks)) << "%" << std::endl;
                }
            }
            
            std::cout << "Long-running work completed!" << std::endl;
            return std::make_shared<StringData>("Work completed successfully");
        });
    }

protected:
    std::future<std::shared_ptr<FrameworkData>> ExecAsyncWithCancellation(
        std::shared_ptr<FrameworkData> PrepRes, const CancellationToken& token) override {
        
        return std::async(std::launch::async, [this, PrepRes, &token]() -> std::shared_ptr<FrameworkData> {
            std::cout << "Starting cancellable long-running work (" << work_duration_.count() << "ms)..." << std::endl;
            
            if (!should_respect_cancellation_) {
                // Simulate a badly behaved operation that doesn't check for cancellation
                std::this_thread::sleep_for(work_duration_);
                return std::make_shared<StringData>("Work completed (ignoring cancellation)");
            }
            
            // Simulate work in small chunks to allow for cancellation checking
            auto chunk_duration = std::chrono::milliseconds(100);
            auto chunks = work_duration_ / chunk_duration;
            
            for (int i = 0; i < static_cast<int>(chunks); ++i) {
                // Check for cancellation before each chunk
                token.throw_if_cancellation_requested();
                
                std::this_thread::sleep_for(chunk_duration);
                
                if (i % 10 == 0) {
                    std::cout << "Progress: " << (i * 100 / static_cast<int>(chunks)) << "%" << std::endl;
                }
            }
            
            std::cout << "Cancellable long-running work completed!" << std::endl;
            return std::make_shared<StringData>("Work completed successfully");
        });
    }

    void cleanup_resources() noexcept override {
        std::cout << "Cleaning up resources for LongRunningAsyncNode..." << std::endl;
        // Simulate cleanup work
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::cout << "Resource cleanup completed." << std::endl;
        } catch (...) {
            std::cout << "Error during resource cleanup (suppressed)." << std::endl;
        }
    }
};

/**
 * @brief Demonstrate basic timeout functionality
 */
void demonstrate_basic_timeout() {
    std::cout << "\n=== Demonstrating Basic Timeout ===\n" << std::endl;
    
    auto node = std::make_shared<LongRunningAsyncNode>(std::chrono::seconds(5));
    auto input_data = std::make_shared<StringData>("test input");
    
    try {
        std::cout << "Starting operation with 2-second timeout..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        
        auto future = node->RunAsyncWithTimeout(input_data, std::chrono::seconds(2));
        auto result = future.get();
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        std::cout << "Operation completed in " << elapsed.count() << "ms" << std::endl;
        
    } catch (const TimeoutException& e) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - std::chrono::steady_clock::now());
        std::cout << "Operation timed out as expected: " << e.what() << std::endl;
        std::cout << "Timeout duration was: " << e.timeout_duration().count() << "ms" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Unexpected error: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrate cancellation functionality
 */
void demonstrate_cancellation() {
    std::cout << "\n=== Demonstrating Cancellation ===\n" << std::endl;
    
    auto node = std::make_shared<LongRunningAsyncNode>(std::chrono::seconds(10), true);
    auto input_data = std::make_shared<StringData>("test input");
    
    CancellationTokenSource token_source;
    
    try {
        std::cout << "Starting cancellable operation..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        
        auto future = node->RunAsyncWithCancellation(input_data, token_source.token());
        
        // Cancel after 2 seconds
        std::thread canceller([&token_source]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "Requesting cancellation..." << std::endl;
            token_source.cancel();
        });
        
        auto result = future.get();
        canceller.join();
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        std::cout << "Operation completed in " << elapsed.count() << "ms" << std::endl;
        
    } catch (const OperationCancelledException& e) {
        std::cout << "Operation cancelled as expected: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Unexpected error: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrate timeout policies
 */
void demonstrate_timeout_policies() {
    std::cout << "\n=== Demonstrating Timeout Policies ===\n" << std::endl;
    
    auto node = std::make_shared<LongRunningAsyncNode>(std::chrono::seconds(2));
    auto input_data = std::make_shared<StringData>("test input");
    
    // Register a custom policy
    TimeoutPolicyRegistry::instance().register_policy(
        "custom_fast", 
        TimeoutPolicy(std::chrono::seconds(1), true, std::chrono::milliseconds(500))
    );
    
    try {
        std::cout << "Using 'fast' timeout policy..." << std::endl;
        auto future = node->RunAsyncWithPolicy(input_data, "fast");
        auto result = future.get();
        std::cout << "Operation completed successfully!" << std::endl;
        
    } catch (const TimeoutException& e) {
        std::cout << "Operation timed out with fast policy: " << e.what() << std::endl;
    } catch (const OperationCancelledException& e) {
        std::cout << "Operation cancelled with fast policy: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error with fast policy: " << e.what() << std::endl;
    }
    
    try {
        std::cout << "\nUsing 'normal' timeout policy..." << std::endl;
        auto future = node->RunAsyncWithPolicy(input_data, "normal");
        auto result = future.get();
        std::cout << "Operation completed successfully with normal policy!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error with normal policy: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrate the execute_with_timeout template function
 */
void demonstrate_execute_with_timeout() {
    std::cout << "\n=== Demonstrating execute_with_timeout Function ===\n" << std::endl;
    
    // Test with a simple function
    auto simple_work = [](int duration_seconds) -> std::string {
        std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
        return "Simple work completed";
    };
    
    try {
        std::cout << "Executing simple work with timeout..." << std::endl;
        auto result = execute_with_timeout(simple_work, std::chrono::seconds(2), 1);
        std::cout << "Result: " << result << std::endl;
    } catch (const TimeoutException& e) {
        std::cout << "Simple work timed out: " << e.what() << std::endl;
    }
    
    // Test with cancellation-aware function
    auto cancellable_work = [](const CancellationToken& token, int chunks) -> std::string {
        for (int i = 0; i < chunks; ++i) {
            token.throw_if_cancellation_requested();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::cout << "Chunk " << (i + 1) << "/" << chunks << " completed" << std::endl;
        }
        return "Cancellable work completed";
    };
    
    try {
        std::cout << "\nExecuting cancellable work with timeout..." << std::endl;
        auto result = execute_with_timeout_and_cancellation(
            cancellable_work, 
            std::chrono::seconds(2), 
            10  // 10 chunks of 500ms each = 5 seconds total
        );
        std::cout << "Result: " << result << std::endl;
    } catch (const TimeoutException& e) {
        std::cout << "Cancellable work timed out: " << e.what() << std::endl;
    } catch (const OperationCancelledException& e) {
        std::cout << "Cancellable work was cancelled: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "AsyncNode Timeout and Cancellation Example" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    try {
        demonstrate_basic_timeout();
        demonstrate_cancellation();
        demonstrate_timeout_policies();
        demonstrate_execute_with_timeout();
        
        std::cout << "\n=== All demonstrations completed ===\n" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}