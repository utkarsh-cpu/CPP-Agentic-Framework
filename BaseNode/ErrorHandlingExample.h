#ifndef ERRORHANDLINGEXAMPLE_H
#define ERRORHANDLINGEXAMPLE_H

#include "FrameworkError.h"
#include "Result.h"
#include "ErrorHandling.h"
#include "FrameworkData.h"
#include <memory>
#include <vector>
#include <thread>
#include <chrono>

namespace framework {

// Example usage of the error handling system
class ErrorHandlingExample {
public:
    // Example 1: Function that can fail with validation
    static Result<int> divide(int numerator, int denominator) {
        if (denominator == 0) {
            return make_error_result<int>(
                FrameworkError::validation_error(
                    "Division by zero is not allowed",
                    FRAMEWORK_ERROR_CONTEXT("divide operation")
                )
            );
        }
        return make_result(numerator / denominator);
    }
    
    // Example 2: Safe container access
    template<typename T>
    static Result<T> get_element_safely(const std::vector<T>& vec, size_t index) {
        auto ptr_result = safe_at(vec, index);
        if (!ptr_result) {
            return make_error_result<T>(std::move(ptr_result).error());
        }
        return make_result(*ptr_result.value());
    }
    
    // Example 3: Chain operations with error propagation
    static Result<std::string> process_data(const std::vector<int>& data, size_t index) {
        // Get element safely
        auto element_result = get_element_safely(data, index);
        if (!element_result) {
            return make_error_result<std::string>(std::move(element_result).error());
        }
        
        // Process the element (divide by 2)
        auto division_result = divide(element_result.value(), 2);
        if (!division_result) {
            return make_error_result<std::string>(std::move(division_result).error());
        }
        
        // Convert to string
        return make_result(std::to_string(division_result.value()));
    }
    
    // Example 4: Using monadic operations
    static Result<std::string> process_data_monadic(const std::vector<int>& data, size_t index) {
        return get_element_safely(data, index)
            .flat_map([](int value) { return divide(value, 2); })
            .map([](int result) { return std::to_string(result); });
    }
    
    // Example 5: Safe FrameworkData casting
    static Result<std::string> extract_string_data(std::shared_ptr<FrameworkData> data) {
        // Validate input
        if (!data) {
            return make_error_result<std::string>(
                FrameworkError::validation_error(
                    "FrameworkData pointer is null",
                    FRAMEWORK_ERROR_CONTEXT("extract_string_data")
                )
            );
        }
        
        // Safe cast to StringData (assuming StringData exists)
        // This is a placeholder - actual StringData implementation would be needed
        auto string_data = data->as<StringData>();
        if (!string_data) {
            return make_error_result<std::string>(
                FrameworkError::validation_error(
                    "FrameworkData is not StringData type",
                    FRAMEWORK_ERROR_CONTEXT("extract_string_data")
                )
            );
        }
        
        // Return the string value (assuming StringData has a 'value' member)
        // return make_result(string_data->value);
        return make_result(std::string("placeholder")); // Placeholder for now
    }
    
    // Example 6: Exception handling with try_execute
    static Result<int> risky_operation(int value) {
        return try_execute([value]() -> int {
            if (value < 0) {
                throw std::invalid_argument("Value must be non-negative");
            }
            if (value > 1000) {
                throw FrameworkError::validation_error("Value too large");
            }
            return value * value;
        });
    }
    
    // Example 7: Timeout handling for async operations
    static Result<int> slow_computation(int value) {
        return execute_with_timeout([value]() -> int {
            // Simulate slow computation
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return value * 2;
        }, std::chrono::milliseconds(200)); // 200ms timeout
    }
    
    // Example 8: Error context chaining
    static Result<std::string> complex_operation(const std::vector<int>& data) {
        try {
            if (data.empty()) {
                throw FrameworkError::validation_error(
                    "Input data is empty",
                    FRAMEWORK_ERROR_CONTEXT("complex_operation - input validation")
                );
            }
            
            auto result = process_data_monadic(data, 0);
            if (!result) {
                // Add context to the existing error
                auto error = std::move(result).error();
                error.add_context(FRAMEWORK_ERROR_CONTEXT("complex_operation - processing failed"));
                return make_error_result<std::string>(std::move(error));
            }
            
            return result;
            
        } catch (const FrameworkError& e) {
            return make_error_result<std::string>(e);
        } catch (const std::exception& e) {
            return make_error_result<std::string>(
                FrameworkError::runtime_error(
                    std::string("Unexpected error in complex_operation: ") + e.what(),
                    FRAMEWORK_ERROR_CONTEXT("complex_operation - exception handler")
                )
            );
        }
    }
};

// Placeholder StringData for the example (would be implemented elsewhere)
struct StringData : public FrameworkData {
    std::string value;
    
    StringData(std::string val) : value(std::move(val)) {}
    
    std::unique_ptr<FrameworkData> clone() const override {
        return std::make_unique<StringData>(value);
    }
    
    const std::type_info& type() const noexcept override {
        return typeid(StringData);
    }
};

} // namespace framework

#endif // ERRORHANDLINGEXAMPLE_H