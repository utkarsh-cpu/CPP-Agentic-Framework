#include <gtest/gtest.h>
#include "BaseNode/FrameworkError.h"
#include "BaseNode/Result.h"
#include "BaseNode/ErrorHandling.h"
#include "AsyncNode/AsyncNode.h"
#include "BaseNode/FrameworkData.h"
#include "Flow/StringData.h"
#include <vector>
#include <memory>
#include <future>
#include <chrono>
#include <thread>
#include <system_error>

using namespace framework;

class ErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }
    
    void TearDown() override {
        // Cleanup
    }
};

// Test FrameworkError basic functionality
TEST_F(ErrorHandlingTest, FrameworkErrorBasicConstruction) {
    FrameworkError error("Test error message", ErrorCategory::RUNTIME_ERROR);
    
    EXPECT_STREQ(error.what(), "Test error message");
    EXPECT_EQ(error.category(), ErrorCategory::RUNTIME_ERROR);
    EXPECT_TRUE(error.is_category(ErrorCategory::RUNTIME_ERROR));
    EXPECT_FALSE(error.is_category(ErrorCategory::LOGIC_ERROR));
}

TEST_F(ErrorHandlingTest, FrameworkErrorWithContext) {
    ErrorContext context("test_function", "test_file.cpp", 42, "Additional info");
    FrameworkError error("Test error with context", ErrorCategory::VALIDATION_ERROR, context);
    
    EXPECT_EQ(error.category(), ErrorCategory::VALIDATION_ERROR);
    EXPECT_EQ(error.context_stack().size(), 1);
    EXPECT_EQ(error.context_stack()[0].function_name, "test_function");
    EXPECT_EQ(error.context_stack()[0].line_number, 42);
}

TEST_F(ErrorHandlingTest, FrameworkErrorFactoryMethods) {
    auto logic_err = FrameworkError::logic_error("Logic error test");
    auto runtime_err = FrameworkError::runtime_error("Runtime error test");
    auto validation_err = FrameworkError::validation_error("Validation error test");
    
    EXPECT_EQ(logic_err.category(), ErrorCategory::LOGIC_ERROR);
    EXPECT_EQ(runtime_err.category(), ErrorCategory::RUNTIME_ERROR);
    EXPECT_EQ(validation_err.category(), ErrorCategory::VALIDATION_ERROR);
}

// Test Result<T> basic functionality
TEST_F(ErrorHandlingTest, ResultWithValue) {
    Result<int> result(42);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result.has_error());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(result.value(), 42);
    EXPECT_EQ(result.value_or(0), 42);
}

TEST_F(ErrorHandlingTest, ResultWithError) {
    FrameworkError error("Test error", ErrorCategory::RUNTIME_ERROR);
    Result<int> result(error);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.has_error());
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_EQ(result.value_or(99), 99);
    EXPECT_EQ(result.error().message(), "Test error");
}

TEST_F(ErrorHandlingTest, ResultValueThrowsOnError) {
    FrameworkError error("Test error", ErrorCategory::RUNTIME_ERROR);
    Result<int> result(error);
    
    EXPECT_THROW(result.value(), FrameworkError);
}

TEST_F(ErrorHandlingTest, ResultErrorThrowsOnValue) {
    Result<int> result(42);
    
    EXPECT_THROW(result.error(), FrameworkError);
}

// Test Result<void> specialization
TEST_F(ErrorHandlingTest, ResultVoidSuccess) {
    Result<void> result;
    
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result.has_error());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_NO_THROW(result.value());
}

TEST_F(ErrorHandlingTest, ResultVoidError) {
    FrameworkError error("Void error test", ErrorCategory::RUNTIME_ERROR);
    Result<void> result(error);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.has_error());
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_THROW(result.value(), FrameworkError);
}

// Test Result monadic operations
TEST_F(ErrorHandlingTest, ResultMapWithValue) {
    Result<int> result(42);
    
    auto mapped = result.map([](int x) { return x * 2; });
    
    EXPECT_TRUE(mapped.has_value());
    EXPECT_EQ(mapped.value(), 84);
}

TEST_F(ErrorHandlingTest, ResultMapWithError) {
    FrameworkError error("Test error", ErrorCategory::RUNTIME_ERROR);
    Result<int> result(error);
    
    auto mapped = result.map([](int x) { return x * 2; });
    
    EXPECT_TRUE(mapped.has_error());
    EXPECT_EQ(mapped.error().message(), "Test error");
}

TEST_F(ErrorHandlingTest, ResultFlatMapWithValue) {
    Result<int> result(42);
    
    auto flat_mapped = result.flat_map([](int x) -> Result<std::string> {
        return make_result(std::to_string(x));
    });
    
    EXPECT_TRUE(flat_mapped.has_value());
    EXPECT_EQ(flat_mapped.value(), "42");
}

TEST_F(ErrorHandlingTest, ResultFlatMapWithError) {
    FrameworkError error("Test error", ErrorCategory::RUNTIME_ERROR);
    Result<int> result(error);
    
    auto flat_mapped = result.flat_map([](int x) -> Result<std::string> {
        return make_result(std::to_string(x));
    });
    
    EXPECT_TRUE(flat_mapped.has_error());
    EXPECT_EQ(flat_mapped.error().message(), "Test error");
}

// Test error handling utilities
TEST_F(ErrorHandlingTest, SafeCastValid) {
    struct Base { virtual ~Base() = default; };
    struct Derived : Base {};
    
    std::unique_ptr<Base> base = std::make_unique<Derived>();
    auto result = safe_cast<Derived>(base.get());
    
    EXPECT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(ErrorHandlingTest, SafeCastInvalid) {
    struct Base { virtual ~Base() = default; };
    struct Other { virtual ~Other() = default; };
    
    std::unique_ptr<Base> base = std::make_unique<Base>();
    auto result = safe_cast<Other>(base.get());
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::VALIDATION_ERROR);
}

TEST_F(ErrorHandlingTest, SafeCastNullPointer) {
    struct Base { virtual ~Base() = default; };
    struct Derived : Base {};
    
    Base* null_ptr = nullptr;
    auto result = safe_cast<Derived>(null_ptr);
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::VALIDATION_ERROR);
}

TEST_F(ErrorHandlingTest, SafeAtValidIndex) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    auto result = safe_at(vec, 2);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
    EXPECT_EQ(*result.value(), 3);
}

TEST_F(ErrorHandlingTest, SafeAtInvalidIndex) {
    std::vector<int> vec = {1, 2, 3};
    auto result = safe_at(vec, 5);
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::VALIDATION_ERROR);
}

TEST_F(ErrorHandlingTest, ValidateNotNullValid) {
    int value = 42;
    int* ptr = &value;
    auto result = validate_not_null(ptr);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(ErrorHandlingTest, ValidateNotNullInvalid) {
    int* null_ptr = nullptr;
    auto result = validate_not_null(null_ptr);
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::VALIDATION_ERROR);
}

TEST_F(ErrorHandlingTest, ValidateRangeValid) {
    auto result = validate_range(5, 1, 10);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 5);
}

TEST_F(ErrorHandlingTest, ValidateRangeInvalid) {
    auto result = validate_range(15, 1, 10);
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::VALIDATION_ERROR);
}

TEST_F(ErrorHandlingTest, TryExecuteSuccess) {
    auto result = try_execute([]() { return 42; });
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(ErrorHandlingTest, TryExecuteWithException) {
    auto result = try_execute([]() -> int {
        throw std::runtime_error("Test exception");
    });
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::RUNTIME_ERROR);
}

TEST_F(ErrorHandlingTest, TryExecuteWithFrameworkError) {
    auto result = try_execute([]() -> int {
        throw FrameworkError::logic_error("Test framework error");
    });
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::LOGIC_ERROR);
}

// Test helper functions
TEST_F(ErrorHandlingTest, MakeResultHelper) {
    auto result = make_result(42);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(ErrorHandlingTest, MakeErrorResultHelper) {
    auto error = FrameworkError::runtime_error("Test error");
    auto result = make_error_result<int>(error);
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().message(), "Test error");
}

TEST_F(ErrorHandlingTest, MakeSuccessHelper) {
    auto result = make_success();
    
    EXPECT_TRUE(result.has_value());
    EXPECT_NO_THROW(result.value());
}

TEST_F(ErrorHandlingTest, MakeErrorHelper) {
    auto error = FrameworkError::runtime_error("Test error");
    auto result = make_error(error);
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().message(), "Test error");
}

// ============================================================================
// ENHANCED ERROR HANDLING TESTS - Task 4.1 Implementation
// ============================================================================

// Test comprehensive FrameworkError construction and information retrieval
TEST_F(ErrorHandlingTest, FrameworkErrorWithErrorCode) {
    std::error_code ec = std::make_error_code(std::errc::invalid_argument);
    FrameworkError error("Error with code", ErrorCategory::VALIDATION_ERROR, ec);
    
    EXPECT_EQ(error.message(), "Error with code");
    EXPECT_EQ(error.category(), ErrorCategory::VALIDATION_ERROR);
    EXPECT_EQ(error.error_code(), ec);
    EXPECT_TRUE(error.is_category(ErrorCategory::VALIDATION_ERROR));
}

TEST_F(ErrorHandlingTest, FrameworkErrorWithErrorCodeAndContext) {
    std::error_code ec = std::make_error_code(std::errc::permission_denied);
    ErrorContext context("test_func", "test.cpp", 123, "Permission denied");
    FrameworkError error("Access denied", ErrorCategory::SYSTEM_ERROR, ec, context);
    
    EXPECT_EQ(error.message(), "Access denied");
    EXPECT_EQ(error.category(), ErrorCategory::SYSTEM_ERROR);
    EXPECT_EQ(error.error_code(), ec);
    EXPECT_EQ(error.context_stack().size(), 1);
    EXPECT_EQ(error.context_stack()[0].function_name, "test_func");
    EXPECT_EQ(error.context_stack()[0].additional_info, "Permission denied");
}

TEST_F(ErrorHandlingTest, FrameworkErrorWithInnerError) {
    auto inner_error = std::make_shared<FrameworkError>(
        FrameworkError::validation_error("Inner validation failed")
    );
    FrameworkError outer_error("Outer operation failed", ErrorCategory::RUNTIME_ERROR, inner_error);
    
    EXPECT_EQ(outer_error.message(), "Outer operation failed");
    EXPECT_EQ(outer_error.category(), ErrorCategory::RUNTIME_ERROR);
    EXPECT_NE(outer_error.inner_error(), nullptr);
    EXPECT_EQ(outer_error.inner_error()->message(), "Inner validation failed");
    EXPECT_EQ(outer_error.inner_error()->category(), ErrorCategory::VALIDATION_ERROR);
}

TEST_F(ErrorHandlingTest, FrameworkErrorContextStack) {
    FrameworkError error("Base error", ErrorCategory::RUNTIME_ERROR);
    
    ErrorContext ctx1("func1", "file1.cpp", 10, "First context");
    ErrorContext ctx2("func2", "file2.cpp", 20, "Second context");
    
    error.add_context(ctx1);
    error.add_context(ctx2);
    
    EXPECT_EQ(error.context_stack().size(), 2);
    EXPECT_EQ(error.context_stack()[0].function_name, "func1");
    EXPECT_EQ(error.context_stack()[1].function_name, "func2");
}

TEST_F(ErrorHandlingTest, FrameworkErrorDetailedFormatting) {
    ErrorContext context("test_function", "/path/to/test.cpp", 42, "Test context");
    std::error_code ec = std::make_error_code(std::errc::no_such_file_or_directory);
    FrameworkError error("File not found", ErrorCategory::SYSTEM_ERROR, ec, context);
    
    std::string detailed = error.format_detailed();
    
    EXPECT_TRUE(detailed.find("[SYSTEM_ERROR]") != std::string::npos);
    EXPECT_TRUE(detailed.find("File not found") != std::string::npos);
    EXPECT_TRUE(detailed.find("test_function") != std::string::npos);
    EXPECT_TRUE(detailed.find("test.cpp:42") != std::string::npos);
    EXPECT_TRUE(detailed.find("Test context") != std::string::npos);
}

TEST_F(ErrorHandlingTest, ErrorCategoryToString) {
    EXPECT_STREQ(to_string(ErrorCategory::LOGIC_ERROR), "LOGIC_ERROR");
    EXPECT_STREQ(to_string(ErrorCategory::RUNTIME_ERROR), "RUNTIME_ERROR");
    EXPECT_STREQ(to_string(ErrorCategory::SYSTEM_ERROR), "SYSTEM_ERROR");
    EXPECT_STREQ(to_string(ErrorCategory::VALIDATION_ERROR), "VALIDATION_ERROR");
    EXPECT_STREQ(to_string(ErrorCategory::CONCURRENCY_ERROR), "CONCURRENCY_ERROR");
    EXPECT_STREQ(to_string(ErrorCategory::RESOURCE_ERROR), "RESOURCE_ERROR");
    EXPECT_STREQ(to_string(ErrorCategory::TIMEOUT_ERROR), "TIMEOUT_ERROR");
    EXPECT_STREQ(to_string(ErrorCategory::CANCELLATION_ERROR), "CANCELLATION_ERROR");
}

// Test comprehensive Result<T> behavior with success and failure cases
TEST_F(ErrorHandlingTest, ResultMoveSemantics) {
    // Test move construction and assignment
    Result<std::string> result1(std::string("Hello World"));
    Result<std::string> result2 = std::move(result1);
    
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), "Hello World");
    
    // Test move assignment
    Result<std::string> result3(FrameworkError::runtime_error("Error"));
    result3 = std::move(result2);
    
    EXPECT_TRUE(result3.has_value());
    EXPECT_EQ(result3.value(), "Hello World");
}

TEST_F(ErrorHandlingTest, ResultMapChaining) {
    Result<int> result(10);
    
    auto chained = result
        .map([](int x) { return x * 2; })
        .map([](int x) { return x + 5; })
        .map([](int x) { return std::to_string(x); });
    
    EXPECT_TRUE(chained.has_value());
    EXPECT_EQ(chained.value(), "25");
}

TEST_F(ErrorHandlingTest, ResultMapChainingWithError) {
    FrameworkError error("Initial error", ErrorCategory::RUNTIME_ERROR);
    Result<int> result(error);
    
    auto chained = result
        .map([](int x) { return x * 2; })
        .map([](int x) { return x + 5; })
        .map([](int x) { return std::to_string(x); });
    
    EXPECT_TRUE(chained.has_error());
    EXPECT_EQ(chained.error().message(), "Initial error");
}

TEST_F(ErrorHandlingTest, ResultMapWithException) {
    Result<int> result(10);
    
    auto mapped = result.map([](int x) -> int {
        if (x > 5) {
            throw std::runtime_error("Value too large");
        }
        return x * 2;
    });
    
    EXPECT_TRUE(mapped.has_error());
    EXPECT_EQ(mapped.error().category(), ErrorCategory::RUNTIME_ERROR);
    EXPECT_TRUE(mapped.error().message().find("Value too large") != std::string::npos);
}

TEST_F(ErrorHandlingTest, ResultMapWithFrameworkException) {
    Result<int> result(10);
    
    auto mapped = result.map([](int x) -> int {
        throw FrameworkError::validation_error("Custom validation error");
    });
    
    EXPECT_TRUE(mapped.has_error());
    EXPECT_EQ(mapped.error().category(), ErrorCategory::VALIDATION_ERROR);
    EXPECT_EQ(mapped.error().message(), "Custom validation error");
}

TEST_F(ErrorHandlingTest, ResultFlatMapChaining) {
    Result<int> result(10);
    
    auto chained = result
        .flat_map([](int x) -> Result<int> {
            if (x < 0) return make_error_result<int>(FrameworkError::validation_error("Negative value"));
            return make_result(x * 2);
        })
        .flat_map([](int x) -> Result<std::string> {
            return make_result(std::to_string(x));
        });
    
    EXPECT_TRUE(chained.has_value());
    EXPECT_EQ(chained.value(), "20");
}

TEST_F(ErrorHandlingTest, ResultFlatMapWithErrorPropagation) {
    Result<int> result(10);
    
    auto chained = result
        .flat_map([](int x) -> Result<int> {
            return make_error_result<int>(FrameworkError::validation_error("Validation failed"));
        })
        .flat_map([](int x) -> Result<std::string> {
            return make_result(std::to_string(x));
        });
    
    EXPECT_TRUE(chained.has_error());
    EXPECT_EQ(chained.error().message(), "Validation failed");
    EXPECT_EQ(chained.error().category(), ErrorCategory::VALIDATION_ERROR);
}

TEST_F(ErrorHandlingTest, ResultMapError) {
    FrameworkError original_error("Original error", ErrorCategory::RUNTIME_ERROR);
    Result<int> result(original_error);
    
    auto mapped = result.map_error([](const FrameworkError& err) {
        return FrameworkError("Transformed: " + err.message(), ErrorCategory::VALIDATION_ERROR);
    });
    
    EXPECT_TRUE(mapped.has_error());
    EXPECT_EQ(mapped.error().message(), "Transformed: Original error");
    EXPECT_EQ(mapped.error().category(), ErrorCategory::VALIDATION_ERROR);
}

TEST_F(ErrorHandlingTest, ResultInspectOperations) {
    bool value_inspected = false;
    bool error_inspected = false;
    
    Result<int> success_result(42);
    success_result
        .inspect([&](int x) { value_inspected = true; EXPECT_EQ(x, 42); })
        .inspect_error([&](const FrameworkError&) { error_inspected = true; });
    
    EXPECT_TRUE(value_inspected);
    EXPECT_FALSE(error_inspected);
    
    value_inspected = false;
    error_inspected = false;
    
    Result<int> error_result(FrameworkError::runtime_error("Test error"));
    error_result
        .inspect([&](int) { value_inspected = true; })
        .inspect_error([&](const FrameworkError& err) { 
            error_inspected = true; 
            EXPECT_EQ(err.message(), "Test error");
        });
    
    EXPECT_FALSE(value_inspected);
    EXPECT_TRUE(error_inspected);
}

// Test timeout functionality
TEST_F(ErrorHandlingTest, ExecuteWithTimeoutSuccess) {
    auto result = execute_with_timeout([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    }, std::chrono::milliseconds(100));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(ErrorHandlingTest, ExecuteWithTimeoutFailure) {
    auto result = execute_with_timeout([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return 42;
    }, std::chrono::milliseconds(50));
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::TIMEOUT_ERROR);
    EXPECT_TRUE(result.error().message().find("timed out") != std::string::npos);
}

TEST_F(ErrorHandlingTest, ExecuteWithTimeoutException) {
    auto result = execute_with_timeout([]() -> int {
        throw std::runtime_error("Test exception");
    }, std::chrono::milliseconds(100));
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::RUNTIME_ERROR);
}

// Test async error propagation through operation chains
class TestAsyncNode : public AsyncNode {
private:
    bool should_fail_prep_ = false;
    bool should_fail_exec_ = false;
    bool should_fail_post_ = false;
    std::string failure_message_;
    
public:
    TestAsyncNode() : AsyncNode() {}
    
    void set_prep_failure(const std::string& message) {
        should_fail_prep_ = true;
        failure_message_ = message;
    }
    
    void set_exec_failure(const std::string& message) {
        should_fail_exec_ = true;
        failure_message_ = message;
    }
    
    void set_post_failure(const std::string& message) {
        should_fail_post_ = true;
        failure_message_ = message;
    }
    
    std::future<std::shared_ptr<FrameworkData>> PrepAsync(std::shared_ptr<FrameworkData> shared) override {
        return std::async(std::launch::async, [this, shared]() -> std::shared_ptr<FrameworkData> {
            if (should_fail_prep_) {
                throw FrameworkError::runtime_error(failure_message_);
            }
            return shared;
        });
    }
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes) override {
        return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
            if (should_fail_exec_) {
                throw FrameworkError::validation_error(failure_message_);
            }
            return PrepRes;
        });
    }
    
    std::future<std::shared_ptr<FrameworkData>> PostAsync(
        std::shared_ptr<FrameworkData> shared,
        std::shared_ptr<FrameworkData> PrepRes,
        std::shared_ptr<FrameworkData> ExecRes) override {
        
        return std::async(std::launch::async, [this, shared, PrepRes, ExecRes]() -> std::shared_ptr<FrameworkData> {
            if (should_fail_post_) {
                throw FrameworkError::resource_error(failure_message_);
            }
            return ExecRes;
        });
    }
};

TEST_F(ErrorHandlingTest, AsyncErrorPropagationPrepPhase) {
    TestAsyncNode node;
    node.set_prep_failure("Preparation failed");
    
    auto shared_data = std::make_shared<StringData>("test data");
    
    // The current AsyncNode implementation catches FrameworkError and re-throws as std::runtime_error
    EXPECT_THROW({
        try {
            auto future = node.RunAsync(shared_data);
            future.get();
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Preparation failed");
            throw;
        }
    }, std::runtime_error);
}

TEST_F(ErrorHandlingTest, AsyncErrorPropagationExecPhase) {
    TestAsyncNode node;
    node.set_exec_failure("Execution failed");
    
    auto shared_data = std::make_shared<StringData>("test data");
    
    // The current AsyncNode implementation catches exceptions in exec() and returns nullptr
    // after exhausting retries and fallback
    EXPECT_NO_THROW({
        auto future = node.RunAsync(shared_data);
        auto result = future.get();
        EXPECT_EQ(result, nullptr); // Should return nullptr when exec fails
    });
}

TEST_F(ErrorHandlingTest, AsyncErrorPropagationPostPhase) {
    TestAsyncNode node;
    node.set_post_failure("Post-processing failed");
    
    auto shared_data = std::make_shared<StringData>("test data");
    
    // The current AsyncNode implementation catches FrameworkError and re-throws as std::runtime_error
    EXPECT_THROW({
        try {
            auto future = node.RunAsync(shared_data);
            future.get();
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Post-processing failed");
            throw;
        }
    }, std::runtime_error);
}

TEST_F(ErrorHandlingTest, AsyncConcurrentExecutionPrevention) {
    TestAsyncNode node;
    auto shared_data = std::make_shared<StringData>("test data");
    
    // Start first async operation
    auto future1 = node.RunAsync(shared_data);
    
    // Try to start second operation while first is running
    EXPECT_THROW({
        auto future2 = node.RunAsync(shared_data);
        future2.get();
    }, std::runtime_error);
    
    // Wait for first operation to complete
    future1.get();
}

// Test error propagation through chained async operations
class ChainedAsyncNode : public AsyncNode {
private:
    std::shared_ptr<AsyncNode> next_node_;
    bool should_fail_ = false;
    std::string failure_message_;
    
public:
    ChainedAsyncNode() : AsyncNode() {}
    
    void set_next_node(std::shared_ptr<AsyncNode> next) {
        next_node_ = next;
    }
    
    void set_failure(const std::string& message) {
        should_fail_ = true;
        failure_message_ = message;
    }
    
    std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes) override {
        return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
            if (should_fail_) {
                throw FrameworkError::runtime_error(failure_message_);
            }
            
            // Chain to next node if available
            if (next_node_) {
                return next_node_->RunAsync(PrepRes).get();
            }
            
            return PrepRes;
        });
    }
};

TEST_F(ErrorHandlingTest, AsyncChainedErrorPropagation) {
    auto node1 = std::make_shared<ChainedAsyncNode>();
    auto node2 = std::make_shared<ChainedAsyncNode>();
    auto node3 = std::make_shared<ChainedAsyncNode>();
    
    // Set up chain: node1 -> node2 -> node3
    node1->set_next_node(node2);
    node2->set_next_node(node3);
    
    // Make node2 fail
    node2->set_failure("Node 2 failed in chain");
    
    auto shared_data = std::make_shared<StringData>("test data");
    
    // The current AsyncNode implementation catches exceptions in exec() and returns nullptr
    // after exhausting retries and fallback
    EXPECT_NO_THROW({
        auto future = node1->RunAsync(shared_data);
        auto result = future.get();
        EXPECT_EQ(result, nullptr); // Should return nullptr when chained node fails
    });
}

TEST_F(ErrorHandlingTest, AsyncChainedSuccessfulExecution) {
    auto node1 = std::make_shared<ChainedAsyncNode>();
    auto node2 = std::make_shared<ChainedAsyncNode>();
    auto node3 = std::make_shared<ChainedAsyncNode>();
    
    // Set up chain: node1 -> node2 -> node3
    node1->set_next_node(node2);
    node2->set_next_node(node3);
    
    auto shared_data = std::make_shared<StringData>("test data");
    
    EXPECT_NO_THROW({
        auto future = node1->RunAsync(shared_data);
        auto result = future.get();
        // The result might be nullptr due to the default implementations returning nullptr
        // This test verifies that the chain executes without throwing exceptions
        // In a real implementation, this would return the processed data
    });
}

// Test exception type conversion in try_execute
TEST_F(ErrorHandlingTest, TryExecuteWithBadAlloc) {
    auto result = try_execute([]() -> int {
        throw std::bad_alloc();
    });
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::RESOURCE_ERROR);
    EXPECT_TRUE(result.error().message().find("Memory allocation failed") != std::string::npos);
}

TEST_F(ErrorHandlingTest, TryExecuteWithBadCast) {
    auto result = try_execute([]() -> int {
        throw std::bad_cast();
    });
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::VALIDATION_ERROR);
    EXPECT_TRUE(result.error().message().find("Type cast failed") != std::string::npos);
}

TEST_F(ErrorHandlingTest, TryExecuteWithSystemError) {
    auto result = try_execute([]() -> int {
        throw std::system_error(std::make_error_code(std::errc::permission_denied), "Access denied");
    });
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::SYSTEM_ERROR);
    EXPECT_TRUE(result.error().message().find("System error") != std::string::npos);
}

TEST_F(ErrorHandlingTest, TryExecuteWithUnknownException) {
    auto result = try_execute([]() -> int {
        throw 42; // Non-standard exception type
    });
    
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().category(), ErrorCategory::RUNTIME_ERROR);
    EXPECT_EQ(result.error().message(), "Unknown exception caught");
}

// ============================================================================
// ADDITIONAL TESTS FOR COMPREHENSIVE ERROR HANDLING COVERAGE
// ============================================================================

// Test error context macro functionality
TEST_F(ErrorHandlingTest, ErrorContextMacro) {
    ErrorContext context = FRAMEWORK_ERROR_CONTEXT("Test context info");
    
    EXPECT_FALSE(context.function_name.empty());
    EXPECT_FALSE(context.file_name.empty());
    EXPECT_GT(context.line_number, 0);
    EXPECT_EQ(context.additional_info, "Test context info");
    EXPECT_GT(context.timestamp.time_since_epoch().count(), 0);
}

// Test error chaining with multiple levels
TEST_F(ErrorHandlingTest, MultiLevelErrorChaining) {
    auto level3_error = std::make_shared<FrameworkError>(
        FrameworkError::validation_error("Level 3 validation failed")
    );
    
    auto level2_error = std::make_shared<FrameworkError>(
        "Level 2 processing failed", ErrorCategory::RUNTIME_ERROR, level3_error
    );
    
    FrameworkError level1_error("Level 1 operation failed", ErrorCategory::SYSTEM_ERROR, level2_error);
    
    EXPECT_EQ(level1_error.message(), "Level 1 operation failed");
    EXPECT_EQ(level1_error.category(), ErrorCategory::SYSTEM_ERROR);
    
    EXPECT_NE(level1_error.inner_error(), nullptr);
    EXPECT_EQ(level1_error.inner_error()->message(), "Level 2 processing failed");
    EXPECT_EQ(level1_error.inner_error()->category(), ErrorCategory::RUNTIME_ERROR);
    
    EXPECT_NE(level1_error.inner_error()->inner_error(), nullptr);
    EXPECT_EQ(level1_error.inner_error()->inner_error()->message(), "Level 3 validation failed");
    EXPECT_EQ(level1_error.inner_error()->inner_error()->category(), ErrorCategory::VALIDATION_ERROR);
}

// Test Result monadic operations with complex chains
TEST_F(ErrorHandlingTest, ResultComplexMonadicChain) {
    auto process_number = [](int x) -> Result<int> {
        if (x < 0) return make_error_result<int>(FrameworkError::validation_error("Negative input"));
        return make_result(x * 2);
    };
    
    auto convert_to_string = [](int x) -> Result<std::string> {
        if (x > 100) return make_error_result<std::string>(FrameworkError::validation_error("Value too large"));
        return make_result(std::to_string(x));
    };
    
    auto add_prefix = [](const std::string& s) -> Result<std::string> {
        return make_result("Result: " + s);
    };
    
    // Test successful chain
    Result<int> input1(10);
    auto result1 = input1
        .flat_map(process_number)
        .flat_map(convert_to_string)
        .flat_map(add_prefix);
    
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), "Result: 20");
    
    // Test chain with early error
    Result<int> input2(-5);
    auto result2 = input2
        .flat_map(process_number)
        .flat_map(convert_to_string)
        .flat_map(add_prefix);
    
    EXPECT_TRUE(result2.has_error());
    EXPECT_EQ(result2.error().message(), "Negative input");
    EXPECT_EQ(result2.error().category(), ErrorCategory::VALIDATION_ERROR);
    
    // Test chain with middle error
    Result<int> input3(60);
    auto result3 = input3
        .flat_map(process_number)
        .flat_map(convert_to_string)
        .flat_map(add_prefix);
    
    EXPECT_TRUE(result3.has_error());
    EXPECT_EQ(result3.error().message(), "Value too large");
    EXPECT_EQ(result3.error().category(), ErrorCategory::VALIDATION_ERROR);
}

// Test Result with custom types
struct TestData {
    int value;
    std::string name;
    
    TestData(int v, std::string n) : value(v), name(std::move(n)) {}
    
    bool operator==(const TestData& other) const {
        return value == other.value && name == other.name;
    }
};

TEST_F(ErrorHandlingTest, ResultWithCustomTypes) {
    Result<TestData> result(TestData{42, "test"});
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().value, 42);
    EXPECT_EQ(result.value().name, "test");
    
    auto mapped = result.map([](const TestData& data) {
        return TestData{data.value * 2, data.name + "_processed"};
    });
    
    EXPECT_TRUE(mapped.has_value());
    EXPECT_EQ(mapped.value().value, 84);
    EXPECT_EQ(mapped.value().name, "test_processed");
}

// Test safe container access with different container types
TEST_F(ErrorHandlingTest, SafeContainerAccessVariousTypes) {
    // Test with std::vector
    std::vector<int> vec = {1, 2, 3, 4, 5};
    auto vec_result = safe_at(vec, 2);
    EXPECT_TRUE(vec_result.has_value());
    EXPECT_EQ(*vec_result.value(), 3);
    
    auto vec_error = safe_at(vec, 10);
    EXPECT_TRUE(vec_error.has_error());
    EXPECT_EQ(vec_error.error().category(), ErrorCategory::VALIDATION_ERROR);
    
    // Test with std::array
    std::array<std::string, 3> arr = {"hello", "world", "test"};
    auto arr_result = safe_at(arr, 1);
    EXPECT_TRUE(arr_result.has_value());
    EXPECT_EQ(*arr_result.value(), "world");
    
    auto arr_error = safe_at(arr, 5);
    EXPECT_TRUE(arr_error.has_error());
    
    // Test with const containers
    const std::vector<double> const_vec = {1.1, 2.2, 3.3};
    auto const_result = safe_at(const_vec, 0);
    EXPECT_TRUE(const_result.has_value());
    EXPECT_DOUBLE_EQ(*const_result.value(), 1.1);
}

// Test validation helpers with edge cases
TEST_F(ErrorHandlingTest, ValidationHelpersEdgeCases) {
    // Test validate_range with boundary values
    auto min_result = validate_range(0, 0, 10);
    EXPECT_TRUE(min_result.has_value());
    EXPECT_EQ(min_result.value(), 0);
    
    auto max_result = validate_range(10, 0, 10);
    EXPECT_TRUE(max_result.has_value());
    EXPECT_EQ(max_result.value(), 10);
    
    auto below_min = validate_range(-1, 0, 10);
    EXPECT_TRUE(below_min.has_error());
    
    auto above_max = validate_range(11, 0, 10);
    EXPECT_TRUE(above_max.has_error());
    
    // Test with floating point values
    auto float_result = validate_range(5.5, 1.0, 10.0);
    EXPECT_TRUE(float_result.has_value());
    EXPECT_DOUBLE_EQ(float_result.value(), 5.5);
}

// Test async timeout with different scenarios
TEST_F(ErrorHandlingTest, AsyncTimeoutVariousScenarios) {
    // Test very short timeout
    auto short_timeout_result = execute_with_timeout([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    }, std::chrono::milliseconds(10));
    
    EXPECT_TRUE(short_timeout_result.has_error());
    EXPECT_EQ(short_timeout_result.error().category(), ErrorCategory::TIMEOUT_ERROR);
    
    // Test immediate return
    auto immediate_result = execute_with_timeout([]() {
        return 123;
    }, std::chrono::milliseconds(1000));
    
    EXPECT_TRUE(immediate_result.has_value());
    EXPECT_EQ(immediate_result.value(), 123);
    
    // Test void function with timeout
    auto void_result = execute_with_timeout([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }, std::chrono::milliseconds(100));
    
    EXPECT_TRUE(void_result.has_value());
}

// Test comprehensive error formatting
TEST_F(ErrorHandlingTest, ComprehensiveErrorFormatting) {
    // Create a complex error with all features
    std::error_code ec = std::make_error_code(std::errc::permission_denied);
    
    auto inner_error = std::make_shared<FrameworkError>(
        FrameworkError::validation_error("Inner validation error", 
                                       FRAMEWORK_ERROR_CONTEXT("Inner context"))
    );
    
    FrameworkError complex_error("Complex operation failed", 
                                ErrorCategory::SYSTEM_ERROR, 
                                ec, 
                                FRAMEWORK_ERROR_CONTEXT("Outer context"));
    
    complex_error.add_context(FRAMEWORK_ERROR_CONTEXT("Additional context 1"));
    complex_error.add_context(FRAMEWORK_ERROR_CONTEXT("Additional context 2"));
    
    std::string formatted = complex_error.format_detailed();
    
    // Verify all components are present in the formatted output
    EXPECT_TRUE(formatted.find("[SYSTEM_ERROR]") != std::string::npos);
    EXPECT_TRUE(formatted.find("Complex operation failed") != std::string::npos);
    EXPECT_TRUE(formatted.find("Permission denied") != std::string::npos);
    EXPECT_TRUE(formatted.find("Context Stack:") != std::string::npos);
    EXPECT_TRUE(formatted.find("Outer context") != std::string::npos);
    EXPECT_TRUE(formatted.find("Additional context 1") != std::string::npos);
    EXPECT_TRUE(formatted.find("Additional context 2") != std::string::npos);
}