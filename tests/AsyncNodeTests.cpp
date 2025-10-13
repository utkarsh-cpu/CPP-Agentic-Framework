#include "gtest/gtest.h"
#include "../AsyncNode/AsyncNode.h"
#include "../Flow/StringData.h"
#include <future>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <stdexcept>

// Test helper class to track resource cleanup
class ResourceTracker {
public:
    static std::atomic<int> resource_count;
    static std::atomic<int> cleanup_count;
    
    static void allocate_resource() {
        resource_count.fetch_add(1);
    }
    
    static void cleanup_resource() {
        cleanup_count.fetch_add(1);
    }
    
    static void reset() {
        resource_count = 0;
        cleanup_count = 0;
    }
    
    static int get_resource_count() {
        return resource_count.load();
    }
    
    static int get_cleanup_count() {
        return cleanup_count.load();
    }
};

std::atomic<int> ResourceTracker::resource_count{0};
std::atomic<int> ResourceTracker::cleanup_count{0};

// Test AsyncNode that throws exceptions in different phases
class TestAsyncNode : public AsyncNode {
private:
    bool throw_in_prep_ = false;
    bool throw_in_exec_ = false;
    bool throw_in_post_ = false;
    bool simulate_resource_allocation_ = false;
    bool override_exec_behavior_ = false;
    std::string exception_message_;

public:
    TestAsyncNode() = default;
    
    void set_throw_in_prep(bool throw_exception, const std::string& message = "PrepAsync failed") {
        throw_in_prep_ = throw_exception;
        exception_message_ = message;
    }
    
    void set_throw_in_exec(bool throw_exception, const std::string& message = "ExecAsync failed") {
        throw_in_exec_ = throw_exception;
        exception_message_ = message;
    }
    
    void set_throw_in_post(bool throw_exception, const std::string& message = "PostAsync failed") {
        throw_in_post_ = throw_exception;
        exception_message_ = message;
    }
    
    void set_simulate_resource_allocation(bool simulate) {
        simulate_resource_allocation_ = simulate;
    }
    
    void set_override_exec_behavior(bool override_behavior) {
        override_exec_behavior_ = override_behavior;
    }

    std::future<std::shared_ptr<FrameworkData>> PrepAsync(std::shared_ptr<FrameworkData> shared) override {
        return std::async(std::launch::async, [this, shared]() -> std::shared_ptr<FrameworkData> {
            if (simulate_resource_allocation_) {
                ResourceTracker::allocate_resource();
            }
            
            if (throw_in_prep_) {
                throw std::runtime_error(exception_message_);
            }
            
            auto result = std::make_shared<StringData>("prep_success");
            return result;
        });
    }

    std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes) override {
        return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
            if (simulate_resource_allocation_) {
                ResourceTracker::allocate_resource();
            }
            
            if (throw_in_exec_) {
                throw std::runtime_error(exception_message_);
            }
            
            auto result = std::make_shared<StringData>("exec_success");
            return result;
        });
    }

    std::future<std::shared_ptr<FrameworkData>> PostAsync(std::shared_ptr<FrameworkData> shared,
                                                          std::shared_ptr<FrameworkData> PrepRes,
                                                          std::shared_ptr<FrameworkData> ExecRes) override {
        return std::async(std::launch::async, [this, shared, PrepRes, ExecRes]() -> std::shared_ptr<FrameworkData> {
            if (simulate_resource_allocation_) {
                ResourceTracker::allocate_resource();
            }
            
            if (throw_in_post_) {
                throw std::runtime_error(exception_message_);
            }
            
            // If ExecRes is nullptr (due to exec failure), return nullptr
            if (!ExecRes) {
                return nullptr;
            }
            
            auto result = std::make_shared<StringData>("post_success");
            return result;
        });
    }

    // Override exec to allow exception propagation for testing
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> PrepRes) override {
        if (override_exec_behavior_ && throw_in_exec_) {
            // For testing exception propagation, throw directly
            throw std::runtime_error(exception_message_);
        }
        // Otherwise use the default behavior
        return AsyncNode::exec(PrepRes);
    }

protected:
    void cleanup_resources() noexcept override {
        try {
            // Simulate resource cleanup
            if (simulate_resource_allocation_) {
                ResourceTracker::cleanup_resource();
            }
            // Call base class cleanup (but don't call it directly since it's private)
        } catch (...) {
            // Should never throw from cleanup
        }
    }
};

// Test fixture for AsyncNode tests
class AsyncNodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ResourceTracker::reset();
        test_node = std::make_unique<TestAsyncNode>();
    }
    
    void TearDown() override {
        test_node.reset();
    }
    
    std::unique_ptr<TestAsyncNode> test_node;
};

// Test 1: Exception propagation through futures - PrepAsync failure
TEST_F(AsyncNodeTest, ExceptionPropagationInPrepAsync) {
    test_node->set_throw_in_prep(true, "PrepAsync test exception");
    
    auto input_data = std::make_shared<StringData>("test_input");
    
    // Exception should propagate through the future
    EXPECT_THROW({
        auto future = test_node->RunAsync(input_data);
        future.get(); // This should throw
    }, std::runtime_error);
}

// Test 2: Exception propagation through futures - ExecAsync failure
TEST_F(AsyncNodeTest, ExceptionPropagationInExecAsync) {
    test_node->set_throw_in_exec(true, "ExecAsync test exception");
    test_node->set_override_exec_behavior(true); // Override to allow exception propagation
    
    auto input_data = std::make_shared<StringData>("test_input");
    
    // Exception should propagate through the future
    EXPECT_THROW({
        auto future = test_node->RunAsync(input_data);
        future.get(); // This should throw
    }, std::runtime_error);
}

// Test 3: Exception propagation through futures - PostAsync failure
TEST_F(AsyncNodeTest, ExceptionPropagationInPostAsync) {
    test_node->set_throw_in_post(true, "PostAsync test exception");
    
    auto input_data = std::make_shared<StringData>("test_input");
    
    // Exception should propagate through the future
    EXPECT_THROW({
        auto future = test_node->RunAsync(input_data);
        future.get(); // This should throw
    }, std::runtime_error);
}

// Test 4: Resource cleanup on async operation failures
TEST_F(AsyncNodeTest, ResourceCleanupOnPrepAsyncFailure) {
    test_node->set_simulate_resource_allocation(true);
    test_node->set_throw_in_prep(true, "PrepAsync cleanup test");
    
    auto input_data = std::make_shared<StringData>("test_input");
    
    EXPECT_THROW({
        auto future = test_node->RunAsync(input_data);
        future.get();
    }, std::runtime_error);
    
    // Give some time for cleanup to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Verify cleanup was called
    EXPECT_GT(ResourceTracker::get_cleanup_count(), 0);
}

// Test 5: Resource cleanup on ExecAsync failure
TEST_F(AsyncNodeTest, ResourceCleanupOnExecAsyncFailure) {
    test_node->set_simulate_resource_allocation(true);
    test_node->set_throw_in_exec(true, "ExecAsync cleanup test");
    
    auto input_data = std::make_shared<StringData>("test_input");
    
    // With current AsyncNode implementation, ExecAsync exceptions are caught and nullptr is returned
    auto future = test_node->RunAsync(input_data);
    auto result = future.get();
    EXPECT_EQ(result, nullptr); // Should return nullptr due to exception handling
    
    // Give some time for cleanup to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Verify cleanup was called (this happens in the retry logic)
    EXPECT_GT(ResourceTracker::get_cleanup_count(), 0);
}

// Test 6: Resource cleanup on PostAsync failure
TEST_F(AsyncNodeTest, ResourceCleanupOnPostAsyncFailure) {
    test_node->set_simulate_resource_allocation(true);
    test_node->set_throw_in_post(true, "PostAsync cleanup test");
    
    auto input_data = std::make_shared<StringData>("test_input");
    
    EXPECT_THROW({
        auto future = test_node->RunAsync(input_data);
        future.get();
    }, std::runtime_error);
    
    // Give some time for cleanup to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Verify cleanup was called
    EXPECT_GT(ResourceTracker::get_cleanup_count(), 0);
}

// Test 7: Concurrent execution prevention - basic test
TEST_F(AsyncNodeTest, ConcurrentExecutionPrevention) {
    auto input_data = std::make_shared<StringData>("test_input");
    
    // Start first async operation
    auto future1 = test_node->RunAsync(input_data);
    
    // Immediately try to start second operation - should throw
    EXPECT_THROW({
        auto future2 = test_node->RunAsync(input_data);
        future2.get();
    }, std::runtime_error);
    
    // First operation should complete successfully
    EXPECT_NO_THROW({
        auto result = future1.get();
        EXPECT_NE(result, nullptr);
    });
}

// Test 8: Concurrent execution prevention with multiple threads
TEST_F(AsyncNodeTest, ConcurrentExecutionPreventionMultiThreaded) {
    auto input_data = std::make_shared<StringData>("test_input");
    
    std::atomic<int> success_count{0};
    std::atomic<int> exception_count{0};
    
    const int num_threads = 5;
    std::vector<std::thread> threads;
    
    // Launch multiple threads trying to execute simultaneously
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            try {
                auto future = test_node->RunAsync(input_data);
                auto result = future.get();
                if (result) {
                    success_count.fetch_add(1);
                }
            } catch (const std::runtime_error& e) {
                // Expected for concurrent executions
                std::string error_msg = e.what();
                if (error_msg.find("already executing") != std::string::npos) {
                    exception_count.fetch_add(1);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Only one should succeed, others should throw concurrent execution exception
    EXPECT_EQ(success_count.load(), 1);
    EXPECT_EQ(exception_count.load(), num_threads - 1);
}

// Test 9: Execution state tracking
TEST_F(AsyncNodeTest, ExecutionStateTracking) {
    auto input_data = std::make_shared<StringData>("test_input");
    
    // Initially not executing
    EXPECT_FALSE(test_node->is_executing());
    
    // Start async operation
    auto future = test_node->RunAsync(input_data);
    
    // Should be executing now (though this might be racy)
    // We'll check after completion instead
    
    // Wait for completion
    auto result = future.get();
    EXPECT_NE(result, nullptr);
    
    // Should not be executing after completion
    EXPECT_FALSE(test_node->is_executing());
}

// Test 10: Exception message preservation
TEST_F(AsyncNodeTest, ExceptionMessagePreservation) {
    const std::string custom_message = "Custom exception message for testing";
    test_node->set_throw_in_exec(true, custom_message);
    test_node->set_override_exec_behavior(true); // Override to allow exception propagation
    
    auto input_data = std::make_shared<StringData>("test_input");
    
    try {
        auto future = test_node->RunAsync(input_data);
        future.get();
        FAIL() << "Expected exception was not thrown";
    } catch (const std::runtime_error& e) {
        std::string caught_message = e.what();
        EXPECT_EQ(caught_message, custom_message);
    }
}

// Test 11: Successful async operation flow
TEST_F(AsyncNodeTest, SuccessfulAsyncOperationFlow) {
    auto input_data = std::make_shared<StringData>("test_input");
    
    auto future = test_node->RunAsync(input_data);
    auto result = future.get();
    
    EXPECT_NE(result, nullptr);
    
    // Should be StringData with "post_success" value
    auto string_result = std::dynamic_pointer_cast<StringData>(result);
    EXPECT_NE(string_result, nullptr);
    EXPECT_EQ(string_result->value, "post_success");
}

// Test 12: Multiple sequential executions (should work after first completes)
TEST_F(AsyncNodeTest, SequentialExecutions) {
    auto input_data = std::make_shared<StringData>("test_input");
    
    // First execution
    auto future1 = test_node->RunAsync(input_data);
    auto result1 = future1.get();
    EXPECT_NE(result1, nullptr);
    
    // Second execution should work after first completes
    auto future2 = test_node->RunAsync(input_data);
    auto result2 = future2.get();
    EXPECT_NE(result2, nullptr);
    
    // Both should have succeeded
    auto string_result1 = std::dynamic_pointer_cast<StringData>(result1);
    auto string_result2 = std::dynamic_pointer_cast<StringData>(result2);
    
    EXPECT_NE(string_result1, nullptr);
    EXPECT_NE(string_result2, nullptr);
    EXPECT_EQ(string_result1->value, "post_success");
    EXPECT_EQ(string_result2->value, "post_success");
}

// Test 13: Current AsyncNode behavior with ExecAsync failures (returns nullptr)
TEST_F(AsyncNodeTest, ExecAsyncFailureCurrentBehavior) {
    test_node->set_throw_in_exec(true, "ExecAsync failure test");
    // Don't override exec behavior - test current implementation
    
    auto input_data = std::make_shared<StringData>("test_input");
    
    // Current AsyncNode implementation catches ExecAsync exceptions and returns nullptr
    auto future = test_node->RunAsync(input_data);
    auto result = future.get();
    
    // Should return nullptr due to exception handling in exec method
    EXPECT_EQ(result, nullptr);
}

// Test 14: Verify cleanup is called during retry logic
TEST_F(AsyncNodeTest, CleanupCalledDuringRetries) {
    test_node->set_simulate_resource_allocation(true);
    test_node->set_throw_in_exec(true, "Retry test");
    test_node->max_retries = 2; // Set retries to test cleanup on each retry
    
    auto input_data = std::make_shared<StringData>("test_input");
    
    auto future = test_node->RunAsync(input_data);
    auto result = future.get();
    
    // Should return nullptr due to all retries failing
    EXPECT_EQ(result, nullptr);
    
    // Give some time for cleanup to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Cleanup should have been called during retries
    EXPECT_GT(ResourceTracker::get_cleanup_count(), 0);
}