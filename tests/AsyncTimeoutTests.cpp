#include <gtest/gtest.h>
#include "../BaseNode/AsyncTimeout.h"
#include "../BaseNode/CancellationToken.h"
#include "../BaseNode/TimeoutPolicy.h"
#include "../AsyncNode/AsyncNode.h"
#include "../Flow/StringData.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <future>
#include <memory>

using namespace framework;

/**
 * @brief Test helper class for simulating work with cancellation support
 */
class CancellableWork {
private:
    std::chrono::milliseconds duration_;
    std::atomic<bool> cleanup_called_{false};
    std::atomic<int> work_chunks_completed_{0};

public:
    explicit CancellableWork(std::chrono::milliseconds duration) : duration_(duration) {}

    // Simulate work that can be cancelled
    std::string do_work(CancellationToken token) {
        auto chunk_duration = std::chrono::milliseconds(50);
        auto total_chunks = duration_ / chunk_duration;
        
        // Register cleanup callback
        token.register_callback([this]() {
            cleanup_called_ = true;
        });
        
        for (int i = 0; i < static_cast<int>(total_chunks); ++i) {
            token.throw_if_cancellation_requested();
            std::this_thread::sleep_for(chunk_duration);
            work_chunks_completed_++;
        }
        
        return "Work completed successfully";
    }

    // Simulate work without cancellation support
    std::string do_work_no_cancellation() {
        std::this_thread::sleep_for(duration_);
        return "Work completed without cancellation";
    }

    bool was_cleanup_called() const { return cleanup_called_; }
    int get_work_chunks_completed() const { return work_chunks_completed_; }
    void reset() { 
        cleanup_called_ = false; 
        work_chunks_completed_ = 0;
    }
};

/**
 * @brief Test AsyncNode that supports cancellation
 */
class CancellableTestNode : public AsyncNode {
private:
    std::shared_ptr<CancellableWork> work_;
    std::atomic<bool> cleanup_performed_{false};

public:
    explicit CancellableTestNode(std::chrono::milliseconds duration)
        : work_(std::make_shared<CancellableWork>(duration)) {}

    std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes) override {
        return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
            auto result = work_->do_work_no_cancellation();
            return std::make_shared<StringData>(result);
        });
    }

protected:
    std::future<std::shared_ptr<FrameworkData>> ExecAsyncWithCancellation(
        std::shared_ptr<FrameworkData> PrepRes, const CancellationToken& token) override {
        
        return std::async(std::launch::async, [this, PrepRes, &token]() -> std::shared_ptr<FrameworkData> {
            try {
                auto result = work_->do_work(token);
                return std::make_shared<StringData>(result);
            } catch (const std::runtime_error&) {
                // Perform cleanup when cancelled
                cleanup_performed_ = true;
                throw OperationCancelledException("Node operation was cancelled");
            }
        });
    }

public:
    bool was_cleanup_performed() const { return cleanup_performed_; }
    std::shared_ptr<CancellableWork> get_work() const { return work_; }
    void reset() { 
        cleanup_performed_ = false; 
        work_->reset();
    }
};

/**
 * @brief Test fixture for timeout and cancellation tests
 */
class TimeoutAndCancellationTest : public ::testing::Test {
protected:
    void SetUp() override {
        short_work_ = std::make_shared<CancellableWork>(std::chrono::milliseconds(100));
        medium_work_ = std::make_shared<CancellableWork>(std::chrono::milliseconds(500));
        long_work_ = std::make_shared<CancellableWork>(std::chrono::milliseconds(2000));
        
        short_node_ = std::make_shared<CancellableTestNode>(std::chrono::milliseconds(100));
        medium_node_ = std::make_shared<CancellableTestNode>(std::chrono::milliseconds(500));
        long_node_ = std::make_shared<CancellableTestNode>(std::chrono::milliseconds(2000));
        
        input_data_ = std::make_shared<StringData>("test input");
    }

    void TearDown() override {
        // Reset all test objects
        if (short_work_) short_work_->reset();
        if (medium_work_) medium_work_->reset();
        if (long_work_) long_work_->reset();
        if (short_node_) short_node_->reset();
        if (medium_node_) medium_node_->reset();
        if (long_node_) long_node_->reset();
    }

    std::shared_ptr<CancellableWork> short_work_;
    std::shared_ptr<CancellableWork> medium_work_;
    std::shared_ptr<CancellableWork> long_work_;
    
    std::shared_ptr<CancellableTestNode> short_node_;
    std::shared_ptr<CancellableTestNode> medium_node_;
    std::shared_ptr<CancellableTestNode> long_node_;
    
    std::shared_ptr<FrameworkData> input_data_;
};

// ============================================================================
// TIMEOUT BEHAVIOR TESTS - Test timeout behavior with operations of varying duration
// ============================================================================

TEST_F(TimeoutAndCancellationTest, ShortOperationCompletesWithinTimeout) {
    // Test that short operations complete successfully within generous timeout
    auto work_func = [this](CancellationToken token) -> std::string {
        return short_work_->do_work(token);
    };

    auto start_time = std::chrono::steady_clock::now();
    std::string result;
    
    EXPECT_NO_THROW({
        result = execute_with_timeout_and_cancellation(
            work_func, 
            std::chrono::seconds(1)  // Generous timeout for 100ms work
        );
    });
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    
    EXPECT_EQ(result, "Work completed successfully");
    EXPECT_LT(elapsed, std::chrono::milliseconds(200));  // Should complete quickly
    EXPECT_FALSE(short_work_->was_cleanup_called());     // No cleanup needed
}

TEST_F(TimeoutAndCancellationTest, MediumOperationTimesOut) {
    // Test that medium operations timeout when given insufficient time
    auto work_func = [this](CancellationToken token) -> std::string {
        return medium_work_->do_work(token);
    };

    auto start_time = std::chrono::steady_clock::now();
    
    EXPECT_THROW({
        execute_with_timeout_and_cancellation(
            work_func,
            std::chrono::milliseconds(200)  // Insufficient timeout for 500ms work
        );
    }, TimeoutException);
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    
    // Should timeout around 200ms, allow some tolerance
    EXPECT_GE(elapsed, std::chrono::milliseconds(180));
    EXPECT_LE(elapsed, std::chrono::milliseconds(300));
    EXPECT_TRUE(medium_work_->was_cleanup_called());  // Cleanup should be called
}

TEST_F(TimeoutAndCancellationTest, LongOperationTimesOutQuickly) {
    // Test that long operations timeout quickly when given very short timeout
    auto work_func = [this](CancellationToken token) -> std::string {
        return long_work_->do_work(token);
    };

    auto start_time = std::chrono::steady_clock::now();
    
    EXPECT_THROW({
        execute_with_timeout_and_cancellation(
            work_func,
            std::chrono::milliseconds(100)  // Very short timeout for 2000ms work
        );
    }, TimeoutException);
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    
    // Should timeout around 100ms
    EXPECT_GE(elapsed, std::chrono::milliseconds(80));
    EXPECT_LE(elapsed, std::chrono::milliseconds(200));
    EXPECT_TRUE(long_work_->was_cleanup_called());
}

TEST_F(TimeoutAndCancellationTest, VaryingTimeoutDurations) {
    // Test multiple timeout durations with the same operation
    std::vector<std::chrono::milliseconds> timeouts = {
        std::chrono::milliseconds(50),   // Too short
        std::chrono::milliseconds(300),  // Still too short
        std::chrono::milliseconds(600),  // Should be enough
        std::chrono::milliseconds(1000)  // Generous
    };
    
    for (auto timeout : timeouts) {
        medium_work_->reset();  // Reset state for each test
        
        auto work_func = [this](CancellationToken token) -> std::string {
            return medium_work_->do_work(token);
        };
        
        auto start_time = std::chrono::steady_clock::now();
        
        if (timeout < std::chrono::milliseconds(500)) {
            // Should timeout
            EXPECT_THROW({
                execute_with_timeout_and_cancellation(work_func, timeout);
            }, TimeoutException) << "Timeout " << timeout.count() << "ms should have failed";
            
            EXPECT_TRUE(medium_work_->was_cleanup_called()) 
                << "Cleanup should be called for timeout " << timeout.count() << "ms";
        } else {
            // Should succeed
            std::string result;
            EXPECT_NO_THROW({
                result = execute_with_timeout_and_cancellation(work_func, timeout);
            }) << "Timeout " << timeout.count() << "ms should have succeeded";
            
            EXPECT_EQ(result, "Work completed successfully");
            EXPECT_FALSE(medium_work_->was_cleanup_called())
                << "Cleanup should not be called for successful timeout " << timeout.count() << "ms";
        }
        
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (timeout < std::chrono::milliseconds(500)) {
            // For timeouts, elapsed should be close to timeout duration
            EXPECT_LE(elapsed, timeout + std::chrono::milliseconds(100));
        }
    }
}

// ============================================================================
// CLEANUP VERIFICATION TESTS - Verify proper cleanup when operations are cancelled
// ============================================================================

TEST_F(TimeoutAndCancellationTest, CleanupCalledOnCancellation) {
    CancellationTokenSource token_source;
    std::atomic<bool> cleanup_executed{false};
    std::atomic<bool> work_started{false};
    
    // Register cleanup callback
    auto token = token_source.token();
    const_cast<CancellationToken&>(token).register_callback([&cleanup_executed]() {
        cleanup_executed = true;
    });
    
    auto work_func = [&](CancellationToken token) -> std::string {
        work_started = true;
        
        // Simulate work that checks for cancellation
        for (int i = 0; i < 100; ++i) {
            token.throw_if_cancellation_requested();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        return "Work completed";
    };
    
    // Start work in background
    auto future = std::async(std::launch::async, [&]() {
        return work_func(token_source.token());
    });
    
    // Wait for work to start, then cancel
    while (!work_started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Let some work happen
    token_source.cancel();
    
    // Verify cancellation exception is thrown
    EXPECT_THROW(future.get(), std::runtime_error);
    EXPECT_TRUE(cleanup_executed);
}

TEST_F(TimeoutAndCancellationTest, ResourceCleanupOnTimeout) {
    std::atomic<int> resources_allocated{0};
    std::atomic<int> resources_cleaned{0};
    
    auto work_func = [&](CancellationToken token) -> std::string {
        // Simulate resource allocation
        resources_allocated = 5;
        
        // Register cleanup
        token.register_callback([&]() {
            resources_cleaned = resources_allocated.load();
            resources_allocated = 0;  // Clean up resources
        });
        
        // Do work that will timeout
        for (int i = 0; i < 1000; ++i) {
            token.throw_if_cancellation_requested();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        return "Work completed";
    };
    
    EXPECT_THROW({
        execute_with_timeout_and_cancellation(
            work_func,
            std::chrono::milliseconds(100)  // Short timeout
        );
    }, TimeoutException);
    
    // Verify resources were cleaned up
    EXPECT_EQ(resources_cleaned.load(), 5);
    EXPECT_EQ(resources_allocated.load(), 0);
}

TEST_F(TimeoutAndCancellationTest, MultipleCleanupCallbacks) {
    CancellationTokenSource token_source;
    std::atomic<int> callback1_called{0};
    std::atomic<int> callback2_called{0};
    std::atomic<int> callback3_called{0};
    
    // Register multiple cleanup callbacks
    auto token = token_source.token();
    const_cast<CancellationToken&>(token).register_callback([&]() { callback1_called++; });
    const_cast<CancellationToken&>(token).register_callback([&]() { callback2_called++; });
    const_cast<CancellationToken&>(token).register_callback([&]() { callback3_called++; });
    
    // Cancel the token
    token_source.cancel();
    
    // All callbacks should be called exactly once
    EXPECT_EQ(callback1_called.load(), 1);
    EXPECT_EQ(callback2_called.load(), 1);
    EXPECT_EQ(callback3_called.load(), 1);
}

TEST_F(TimeoutAndCancellationTest, CleanupCallbackExceptionHandling) {
    CancellationTokenSource token_source;
    std::atomic<bool> good_callback_called{false};
    
    // Register callbacks, one that throws and one that doesn't
    auto token = token_source.token();
    const_cast<CancellationToken&>(token).register_callback([]() {
        throw std::runtime_error("Callback exception");
    });
    
    const_cast<CancellationToken&>(token).register_callback([&]() {
        good_callback_called = true;
    });
    
    // Cancel should not throw even if callbacks throw
    EXPECT_NO_THROW(token_source.cancel());
    
    // Good callback should still be called despite exception in other callback
    EXPECT_TRUE(good_callback_called);
}

// ============================================================================
// CANCELLATION TOKEN PROPAGATION TESTS - Test cancellation token propagation through operation chains
// ============================================================================

TEST_F(TimeoutAndCancellationTest, TokenPropagationThroughChain) {
    CancellationTokenSource root_token_source;
    std::atomic<int> operations_started{0};
    std::atomic<int> operations_cancelled{0};
    
    // Create a chain of operations that pass the token
    auto operation1 = [&](CancellationToken token) -> std::string {
        operations_started++;
        token.register_callback([&]() { operations_cancelled++; });
        
        // Simulate some work
        for (int i = 0; i < 50; ++i) {
            token.throw_if_cancellation_requested();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        
        return "Op1 completed";
    };
    
    auto operation2 = [&](CancellationToken token, const std::string& input) -> std::string {
        operations_started++;
        token.register_callback([&]() { operations_cancelled++; });
        
        // Simulate some work
        for (int i = 0; i < 50; ++i) {
            token.throw_if_cancellation_requested();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        
        return input + " -> Op2 completed";
    };
    
    auto operation3 = [&](CancellationToken token, const std::string& input) -> std::string {
        operations_started++;
        token.register_callback([&]() { operations_cancelled++; });
        
        // Simulate some work
        for (int i = 0; i < 50; ++i) {
            token.throw_if_cancellation_requested();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        
        return input + " -> Op3 completed";
    };
    
    // Start the operation chain
    auto future = std::async(std::launch::async, [&]() {
        auto result1 = operation1(root_token_source.token());
        auto result2 = operation2(root_token_source.token(), result1);
        auto result3 = operation3(root_token_source.token(), result2);
        return result3;
    });
    
    // Let some operations start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Cancel the root token
    root_token_source.cancel();
    
    // Verify cancellation propagated
    EXPECT_THROW(future.get(), std::runtime_error);
    EXPECT_GT(operations_started.load(), 0);  // At least some operations started
    EXPECT_EQ(operations_cancelled.load(), operations_started.load());  // All started operations were cancelled
}

TEST_F(TimeoutAndCancellationTest, NestedOperationCancellation) {
    CancellationTokenSource parent_token_source;
    std::atomic<bool> parent_cancelled{false};
    std::atomic<bool> child_cancelled{false};
    
    auto parent_operation = [&](CancellationToken parent_token) -> std::string {
        parent_token.register_callback([&]() { parent_cancelled = true; });
        
        // Create child operation with its own token source
        CancellationTokenSource child_token_source;
        auto child_token = child_token_source.token();
        const_cast<CancellationToken&>(child_token).register_callback([&]() { child_cancelled = true; });
        
        // Parent cancellation should trigger child cancellation
        parent_token.register_callback([&child_token_source]() {
            child_token_source.cancel();
        });
        
        auto child_operation = [&](CancellationToken child_token) -> std::string {
            for (int i = 0; i < 1000; ++i) {
                child_token.throw_if_cancellation_requested();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return "Child completed";
        };
        
        // Start child operation
        return child_operation(child_token);
    };
    
    // Start parent operation
    auto future = std::async(std::launch::async, [&]() {
        return parent_operation(parent_token_source.token());
    });
    
    // Let operation start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Cancel parent
    parent_token_source.cancel();
    
    // Verify both parent and child were cancelled
    EXPECT_THROW(future.get(), std::runtime_error);
    EXPECT_TRUE(parent_cancelled);
    EXPECT_TRUE(child_cancelled);
}

TEST_F(TimeoutAndCancellationTest, AsyncNodeCancellationChain) {
    // Test cancellation propagation through AsyncNode operations
    auto start_time = std::chrono::steady_clock::now();
    
    // Use long-running node that should be cancelled
    EXPECT_THROW({
        execute_with_timeout_and_cancellation(
            [this](CancellationToken token) -> std::string {
                return long_node_->get_work()->do_work(token);
            },
            std::chrono::milliseconds(200)  // Short timeout for long operation
        );
    }, TimeoutException);
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    
    // Should timeout quickly
    EXPECT_LE(elapsed, std::chrono::milliseconds(300));
    EXPECT_TRUE(long_node_->get_work()->was_cleanup_called());
}

// ============================================================================
// EDGE CASES AND ERROR HANDLING TESTS
// ============================================================================

TEST_F(TimeoutAndCancellationTest, AlreadyCancelledToken) {
    // Test behavior with pre-cancelled token
    auto cancelled_token = CancellationToken::cancelled();
    
    EXPECT_TRUE(cancelled_token.is_cancellation_requested());
    EXPECT_THROW(cancelled_token.throw_if_cancellation_requested(), std::runtime_error);
    
    // Callback should be executed immediately on already-cancelled token
    bool callback_called = false;
    const_cast<CancellationToken&>(cancelled_token).register_callback([&]() {
        callback_called = true;
    });
    EXPECT_TRUE(callback_called);
}

TEST_F(TimeoutAndCancellationTest, ZeroTimeout) {
    // Test behavior with zero timeout
    auto work_func = [](CancellationToken token) -> std::string {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return "Should not complete";
    };
    
    EXPECT_THROW({
        execute_with_timeout_and_cancellation(work_func, std::chrono::milliseconds(0));
    }, TimeoutException);
}

TEST_F(TimeoutAndCancellationTest, ExceptionInWorkFunction) {
    // Test that exceptions in work function are properly propagated
    auto work_func = [](CancellationToken token) -> std::string {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        throw std::logic_error("Work function error");
    };
    
    EXPECT_THROW({
        execute_with_timeout_and_cancellation(
            work_func, 
            std::chrono::seconds(1)  // Generous timeout
        );
    }, std::logic_error);
}

TEST_F(TimeoutAndCancellationTest, TimeoutExceptionDetails) {
    // Test TimeoutException contains correct information
    try {
        execute_with_timeout_and_cancellation(
            [](CancellationToken token) -> std::string {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                return "Should not complete";
            },
            std::chrono::milliseconds(100)
        );
        FAIL() << "Expected TimeoutException";
    } catch (const TimeoutException& ex) {
        EXPECT_STREQ(ex.what(), "Operation timed out");
        EXPECT_EQ(ex.timeout_duration(), std::chrono::milliseconds(100));
        EXPECT_EQ(ex.category(), ErrorCategory::TIMEOUT_ERROR);
    }
}

TEST_F(TimeoutAndCancellationTest, OperationCancelledExceptionDetails) {
    // Test OperationCancelledException contains correct information
    CancellationTokenSource token_source;
    
    auto future = std::async(std::launch::async, [&]() {
        try {
            long_work_->do_work(token_source.token());
        } catch (const std::runtime_error&) {
            throw OperationCancelledException("Custom cancellation message");
        }
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    token_source.cancel();
    
    try {
        future.get();
        FAIL() << "Expected OperationCancelledException";
    } catch (const OperationCancelledException& ex) {
        EXPECT_STREQ(ex.what(), "Custom cancellation message");
        EXPECT_EQ(ex.category(), ErrorCategory::CANCELLATION_ERROR);
    }
}