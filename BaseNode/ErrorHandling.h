#ifndef ERRORHANDLING_H
#define ERRORHANDLING_H

#include "FrameworkError.h"
#include "Result.h"
#include <future>
#include <chrono>
#include <functional>

namespace framework {

// Timeout handling for async operations
template<typename F>
auto execute_with_timeout(F&& func, std::chrono::milliseconds timeout) 
    -> Result<std::invoke_result_t<F>> {
    
    using ReturnType = std::invoke_result_t<F>;
    
    try {
        auto future = std::async(std::launch::async, std::forward<F>(func));
        
        if (future.wait_for(timeout) == std::future_status::timeout) {
            return make_error_result<ReturnType>(
                FrameworkError::timeout_error(
                    "Operation timed out after " + std::to_string(timeout.count()) + "ms",
                    FRAMEWORK_ERROR_CONTEXT("execute_with_timeout")
                )
            );
        }
        
        if constexpr (std::is_void_v<ReturnType>) {
            future.get();
            return make_success();
        } else {
            return make_result(future.get());
        }
        
    } catch (const FrameworkError& e) {
        return make_error_result<ReturnType>(e);
    } catch (const std::exception& e) {
        return make_error_result<ReturnType>(
            FrameworkError::runtime_error(
                std::string("Async operation failed: ") + e.what(),
                FRAMEWORK_ERROR_CONTEXT("execute_with_timeout")
            )
        );
    }
}

// Safe casting with Result return type
template<typename To, typename From>
[[nodiscard]] Result<To*> safe_cast(From* ptr) noexcept {
    if (!ptr) {
        return make_error_result<To*>(
            FrameworkError::validation_error(
                "Cannot cast null pointer",
                FRAMEWORK_ERROR_CONTEXT("safe_cast")
            )
        );
    }
    
    To* result = dynamic_cast<To*>(ptr);
    if (!result) {
        return make_error_result<To*>(
            FrameworkError::validation_error(
                "Invalid cast from " + std::string(typeid(From).name()) + 
                " to " + std::string(typeid(To).name()),
                FRAMEWORK_ERROR_CONTEXT("safe_cast")
            )
        );
    }
    
    return make_result(result);
}

template<typename To, typename From>
[[nodiscard]] Result<const To*> safe_cast(const From* ptr) noexcept {
    if (!ptr) {
        return make_error_result<const To*>(
            FrameworkError::validation_error(
                "Cannot cast null pointer",
                FRAMEWORK_ERROR_CONTEXT("safe_cast")
            )
        );
    }
    
    const To* result = dynamic_cast<const To*>(ptr);
    if (!result) {
        return make_error_result<const To*>(
            FrameworkError::validation_error(
                "Invalid cast from " + std::string(typeid(From).name()) + 
                " to " + std::string(typeid(To).name()),
                FRAMEWORK_ERROR_CONTEXT("safe_cast")
            )
        );
    }
    
    return make_result(result);
}

// Bounds checking for containers - returns pointer to avoid reference issues
template<typename Container>
[[nodiscard]] Result<typename Container::value_type*> 
safe_at(Container& container, typename Container::size_type index) {
    if (index >= container.size()) {
        return make_error_result<typename Container::value_type*>(
            FrameworkError::validation_error(
                "Index " + std::to_string(index) + " out of bounds (size: " + 
                std::to_string(container.size()) + ")",
                FRAMEWORK_ERROR_CONTEXT("safe_at")
            )
        );
    }
    
    return make_result(&container[index]);
}

template<typename Container>
[[nodiscard]] Result<const typename Container::value_type*> 
safe_at(const Container& container, typename Container::size_type index) {
    if (index >= container.size()) {
        return make_error_result<const typename Container::value_type*>(
            FrameworkError::validation_error(
                "Index " + std::to_string(index) + " out of bounds (size: " + 
                std::to_string(container.size()) + ")",
                FRAMEWORK_ERROR_CONTEXT("safe_at")
            )
        );
    }
    
    return make_result(&container[index]);
}

// Input validation helpers
template<typename T>
[[nodiscard]] Result<T> validate_not_null(T* ptr, const std::string& param_name = "parameter") {
    if (!ptr) {
        return make_error_result<T>(
            FrameworkError::validation_error(
                param_name + " cannot be null",
                FRAMEWORK_ERROR_CONTEXT("validate_not_null")
            )
        );
    }
    return make_result(*ptr);
}

template<typename T>
[[nodiscard]] Result<T> validate_range(T value, T min_val, T max_val, 
                                      const std::string& param_name = "value") {
    if (value < min_val || value > max_val) {
        return make_error_result<T>(
            FrameworkError::validation_error(
                param_name + " (" + std::to_string(value) + ") out of range [" +
                std::to_string(min_val) + ", " + std::to_string(max_val) + "]",
                FRAMEWORK_ERROR_CONTEXT("validate_range")
            )
        );
    }
    return make_result(value);
}

// Exception to Result conversion
template<typename F>
[[nodiscard]] auto try_execute(F&& func) -> Result<std::invoke_result_t<F>> {
    using ReturnType = std::invoke_result_t<F>;
    
    try {
        if constexpr (std::is_void_v<ReturnType>) {
            std::invoke(std::forward<F>(func));
            return make_success();
        } else {
            return make_result(std::invoke(std::forward<F>(func)));
        }
    } catch (const FrameworkError& e) {
        return make_error_result<ReturnType>(e);
    } catch (const std::bad_alloc& e) {
        return make_error_result<ReturnType>(
            FrameworkError::resource_error(
                std::string("Memory allocation failed: ") + e.what(),
                FRAMEWORK_ERROR_CONTEXT("try_execute")
            )
        );
    } catch (const std::bad_cast& e) {
        return make_error_result<ReturnType>(
            FrameworkError::validation_error(
                std::string("Type cast failed: ") + e.what(),
                FRAMEWORK_ERROR_CONTEXT("try_execute")
            )
        );
    } catch (const std::system_error& e) {
        return make_error_result<ReturnType>(
            FrameworkError(
                std::string("System error: ") + e.what(),
                ErrorCategory::SYSTEM_ERROR,
                e.code(),
                FRAMEWORK_ERROR_CONTEXT("try_execute")
            )
        );
    } catch (const std::exception& e) {
        return make_error_result<ReturnType>(
            FrameworkError::runtime_error(
                std::string("Unexpected error: ") + e.what(),
                FRAMEWORK_ERROR_CONTEXT("try_execute")
            )
        );
    } catch (...) {
        return make_error_result<ReturnType>(
            FrameworkError::runtime_error(
                "Unknown exception caught",
                FRAMEWORK_ERROR_CONTEXT("try_execute")
            )
        );
    }
}

// Macro helpers for common patterns
#define FRAMEWORK_TRY(expr) \
    do { \
        auto result = (expr); \
        if (!result) { \
            return make_error_result<decltype(result)::value_type>(std::move(result).error()); \
        } \
    } while(0)

#define FRAMEWORK_TRY_ASSIGN(var, expr) \
    do { \
        auto result = (expr); \
        if (!result) { \
            return make_error_result<std::decay_t<decltype(var)>>(std::move(result).error()); \
        } \
        var = std::move(result).value(); \
    } while(0)

#define FRAMEWORK_PROPAGATE_ERROR(result) \
    do { \
        if (!(result)) { \
            return make_error_result<decltype((result))::value_type>(std::move(result).error()); \
        } \
    } while(0)

} // namespace framework

#endif // ERRORHANDLING_H