#ifndef ASYNCTIMEOUT_H
#define ASYNCTIMEOUT_H

#include "CancellationToken.h"
#include "TimeoutPolicy.h"
#include "FrameworkError.h"
#include <future>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <type_traits>

using namespace framework;

/**
 * @brief Exception thrown when an operation times out
 */
class TimeoutException : public FrameworkError {
public:
    explicit TimeoutException(const std::string& message, std::chrono::milliseconds timeout_duration)
        : FrameworkError(message, ErrorCategory::TIMEOUT_ERROR, std::make_error_code(std::errc::timed_out))
        , timeout_duration_(timeout_duration) {}

    [[nodiscard]] std::chrono::milliseconds timeout_duration() const noexcept {
        return timeout_duration_;
    }

private:
    std::chrono::milliseconds timeout_duration_;
};

/**
 * @brief Exception thrown when an operation is cancelled
 */
class OperationCancelledException : public FrameworkError {
public:
    explicit OperationCancelledException(const std::string& message = "Operation was cancelled")
        : FrameworkError(message, ErrorCategory::CANCELLATION_ERROR, std::make_error_code(std::errc::operation_canceled)) {}
};

/**
 * @brief Result of a timeout operation that includes timing information
 */
template<typename T>
struct TimeoutResult {
    T result;
    std::chrono::milliseconds elapsed_time;
    bool timed_out;
    bool cancelled;

    TimeoutResult(T&& res, std::chrono::milliseconds elapsed, bool timeout, bool cancel)
        : result(std::forward<T>(res)), elapsed_time(elapsed), timed_out(timeout), cancelled(cancel) {}
};

/**
 * @brief Execute a function with timeout support
 * @tparam F Function type
 * @tparam Args Argument types
 * @param func Function to execute
 * @param timeout Maximum time to wait
 * @param args Arguments to pass to the function
 * @return Result of the function execution
 * @throws TimeoutException if the operation times out
 */
template<typename F, typename... Args>
auto execute_with_timeout(F&& func, std::chrono::milliseconds timeout, Args&&... args) 
    -> std::invoke_result_t<F, Args...> {
    
    using ReturnType = std::invoke_result_t<F, Args...>;
    
    // Create a promise/future pair for the result
    std::promise<ReturnType> promise;
    auto future = promise.get_future();
    
    // Launch the function asynchronously
    std::thread worker([&promise, func = std::forward<F>(func), args...]() mutable {
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                func(args...);
                promise.set_value();
            } else {
                auto result = func(args...);
                promise.set_value(std::move(result));
            }
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    });
    
    // Wait for completion or timeout
    auto status = future.wait_for(timeout);
    
    if (status == std::future_status::timeout) {
        // Operation timed out - we can't safely cancel the worker thread,
        // so we detach it and throw an exception
        worker.detach();
        throw TimeoutException("Operation timed out", timeout);
    }
    
    // Operation completed, join the worker thread
    worker.join();
    
    // Return the result (this will throw if the function threw an exception)
    if constexpr (std::is_void_v<ReturnType>) {
        future.get();
    } else {
        return future.get();
    }
}

/**
 * @brief Execute a function with timeout and cancellation support
 * @tparam F Function type
 * @tparam Args Argument types
 * @param func Function to execute (must accept CancellationToken as first parameter)
 * @param timeout Maximum time to wait
 * @param args Arguments to pass to the function (after cancellation token)
 * @return Result of the function execution
 * @throws TimeoutException if the operation times out
 * @throws OperationCancelledException if the operation is cancelled
 */
template<typename F, typename... Args>
auto execute_with_timeout_and_cancellation(F&& func, std::chrono::milliseconds timeout, Args&&... args)
    -> std::invoke_result_t<F, CancellationToken, Args...> {
    
    using ReturnType = std::invoke_result_t<F, CancellationToken, Args...>;
    
    // Create cancellation token source
    CancellationTokenSource token_source;
    
    // Create a promise/future pair for the result
    std::promise<ReturnType> promise;
    auto future = promise.get_future();
    
    // Launch the function asynchronously with cancellation token
    std::thread worker([&promise, &token_source, func = std::forward<F>(func), args...]() mutable {
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                func(token_source.token(), args...);
                promise.set_value();
            } else {
                auto result = func(token_source.token(), args...);
                promise.set_value(std::move(result));
            }
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    });
    
    // Wait for completion or timeout
    auto status = future.wait_for(timeout);
    
    if (status == std::future_status::timeout) {
        // Cancel the operation
        token_source.cancel();
        
        // Give the operation a chance to clean up
        auto cleanup_timeout = std::chrono::seconds(5);
        if (future.wait_for(cleanup_timeout) == std::future_status::timeout) {
            // Cleanup timed out, detach the worker thread
            worker.detach();
            throw TimeoutException("Operation timed out and cleanup failed", timeout);
        } else {
            // Cleanup completed, join the worker thread
            worker.join();
            throw TimeoutException("Operation timed out", timeout);
        }
    }
    
    // Operation completed, join the worker thread
    worker.join();
    
    // Return the result (this will throw if the function threw an exception)
    if constexpr (std::is_void_v<ReturnType>) {
        future.get();
    } else {
        return future.get();
    }
}

/**
 * @brief Execute a function with a timeout policy
 * @tparam F Function type
 * @tparam Args Argument types
 * @param func Function to execute
 * @param policy Timeout policy to use
 * @param args Arguments to pass to the function
 * @return Result of the function execution
 */
template<typename F, typename... Args>
auto execute_with_policy(F&& func, const TimeoutPolicy& policy, Args&&... args) {
    if (policy.cancellation_enabled()) {
        return execute_with_timeout_and_cancellation(
            std::forward<F>(func), 
            policy.timeout(), 
            std::forward<Args>(args)...
        );
    } else {
        return execute_with_timeout(
            std::forward<F>(func), 
            policy.timeout(), 
            std::forward<Args>(args)...
        );
    }
}

/**
 * @brief Execute a function with a named timeout policy
 * @tparam F Function type
 * @tparam Args Argument types
 * @param func Function to execute
 * @param policy_name Name of the timeout policy to use
 * @param args Arguments to pass to the function
 * @return Result of the function execution
 */
template<typename F, typename... Args>
auto execute_with_named_policy(F&& func, const std::string& policy_name, Args&&... args) {
    auto policy = TimeoutPolicyRegistry::instance().get_policy(policy_name);
    return execute_with_policy(std::forward<F>(func), policy, std::forward<Args>(args)...);
}

/**
 * @brief Execute a function with detailed timeout result information
 * @tparam F Function type
 * @tparam Args Argument types
 * @param func Function to execute
 * @param timeout Maximum time to wait
 * @param args Arguments to pass to the function
 * @return TimeoutResult containing the result and timing information
 */
template<typename F, typename... Args>
auto execute_with_timeout_detailed(F&& func, std::chrono::milliseconds timeout, Args&&... args)
    -> TimeoutResult<std::invoke_result_t<F, Args...>> {
    
    using ReturnType = std::invoke_result_t<F, Args...>;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        if constexpr (std::is_void_v<ReturnType>) {
            execute_with_timeout(std::forward<F>(func), timeout, std::forward<Args>(args)...);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);
            return TimeoutResult<ReturnType>({}, elapsed, false, false);
        } else {
            auto result = execute_with_timeout(std::forward<F>(func), timeout, std::forward<Args>(args)...);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);
            return TimeoutResult<ReturnType>(std::move(result), elapsed, false, false);
        }
    } catch (const TimeoutException&) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        if constexpr (std::is_void_v<ReturnType>) {
            return TimeoutResult<ReturnType>({}, elapsed, true, false);
        } else {
            throw; // Re-throw for non-void types since we can't construct a default result
        }
    } catch (const OperationCancelledException&) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        if constexpr (std::is_void_v<ReturnType>) {
            return TimeoutResult<ReturnType>({}, elapsed, false, true);
        } else {
            throw; // Re-throw for non-void types since we can't construct a default result
        }
    }
}

#endif // ASYNCTIMEOUT_H