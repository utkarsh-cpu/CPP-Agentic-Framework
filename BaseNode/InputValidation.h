#ifndef INPUT_VALIDATION_H
#define INPUT_VALIDATION_H

#include <string>
#include <stdexcept>
#include <limits>
#include <type_traits>
#include <concepts>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <regex>
#include <cmath>

namespace Security {

// Custom exception types for security violations
class SecurityException : public std::runtime_error {
public:
    explicit SecurityException(const std::string& message) 
        : std::runtime_error("Security violation: " + message) {}
};

class BoundsCheckException : public SecurityException {
public:
    explicit BoundsCheckException(const std::string& message) 
        : SecurityException("Bounds check failed: " + message) {}
};

class InputValidationException : public SecurityException {
public:
    explicit InputValidationException(const std::string& message) 
        : SecurityException("Input validation failed: " + message) {}
};

class IntegerOverflowException : public SecurityException {
public:
    explicit IntegerOverflowException(const std::string& message) 
        : SecurityException("Integer overflow detected: " + message) {}
};

class UnsafeCastException : public SecurityException {
public:
    explicit UnsafeCastException(const std::string& message) 
        : SecurityException("Unsafe cast detected: " + message) {}
};

// Bounds checking utilities
class BoundsChecker {
public:
    // Check array/vector bounds
    template<typename Container>
    static void check_bounds(const Container& container, size_t index, const std::string& context = "") {
        if (index >= container.size()) {
            throw BoundsCheckException(
                "Index " + std::to_string(index) + 
                " out of bounds for container of size " + std::to_string(container.size()) +
                (context.empty() ? "" : " in " + context)
            );
        }
    }
    
    // Check range bounds
    template<typename T>
    static void check_range(T value, T min_val, T max_val, const std::string& context = "") 
        requires std::totally_ordered<T> {
        if (value < min_val || value > max_val) {
            throw BoundsCheckException(
                "Value " + std::to_string(value) + 
                " out of range [" + std::to_string(min_val) + ", " + std::to_string(max_val) + "]" +
                (context.empty() ? "" : " in " + context)
            );
        }
    }
    
    // Check pointer bounds
    template<typename T>
    static void check_not_null(const T* ptr, const std::string& context = "") {
        if (ptr == nullptr) {
            throw BoundsCheckException("Null pointer access" + (context.empty() ? "" : " in " + context));
        }
    }
    
    // Check shared_ptr bounds
    template<typename T>
    static void check_not_null(const std::shared_ptr<T>& ptr, const std::string& context = "") {
        if (!ptr) {
            throw BoundsCheckException("Null shared_ptr access" + (context.empty() ? "" : " in " + context));
        }
    }
    
    // Check unique_ptr bounds
    template<typename T>
    static void check_not_null(const std::unique_ptr<T>& ptr, const std::string& context = "") {
        if (!ptr) {
            throw BoundsCheckException("Null unique_ptr access" + (context.empty() ? "" : " in " + context));
        }
    }
    
    // Check string bounds for substring operations
    static void check_string_bounds(const std::string& str, size_t pos, size_t len = std::string::npos, 
                                   const std::string& context = "") {
        if (pos > str.length()) {
            throw BoundsCheckException(
                "String position " + std::to_string(pos) + 
                " out of bounds for string of length " + std::to_string(str.length()) +
                (context.empty() ? "" : " in " + context)
            );
        }
        if (len != std::string::npos && pos + len > str.length()) {
            throw BoundsCheckException(
                "String range [" + std::to_string(pos) + ", " + std::to_string(pos + len) + 
                ") out of bounds for string of length " + std::to_string(str.length()) +
                (context.empty() ? "" : " in " + context)
            );
        }
    }
};

// Input validation utilities
class InputValidator {
public:
    // String validation
    static void validate_non_empty_string(const std::string& str, const std::string& field_name = "string") {
        if (str.empty()) {
            throw InputValidationException(field_name + " cannot be empty");
        }
    }
    
    static void validate_string_length(const std::string& str, size_t max_length, 
                                     const std::string& field_name = "string") {
        if (str.length() > max_length) {
            throw InputValidationException(
                field_name + " length " + std::to_string(str.length()) + 
                " exceeds maximum " + std::to_string(max_length)
            );
        }
    }
    
    static void validate_string_pattern(const std::string& str, const std::regex& pattern, 
                                      const std::string& field_name = "string") {
        if (!std::regex_match(str, pattern)) {
            throw InputValidationException(field_name + " does not match required pattern");
        }
    }
    
    // Numeric validation
    template<typename T>
    static void validate_positive(T value, const std::string& field_name = "value") 
        requires std::is_arithmetic_v<T> {
        if (value <= 0) {
            throw InputValidationException(field_name + " must be positive");
        }
    }
    
    template<typename T>
    static void validate_non_negative(T value, const std::string& field_name = "value") 
        requires std::is_arithmetic_v<T> {
        if (value < 0) {
            throw InputValidationException(field_name + " must be non-negative");
        }
    }
    
    template<typename T>
    static void validate_range(T value, T min_val, T max_val, const std::string& field_name = "value") 
        requires std::totally_ordered<T> {
        if (value < min_val || value > max_val) {
            throw InputValidationException(
                field_name + " " + std::to_string(value) + 
                " is out of valid range [" + std::to_string(min_val) + ", " + std::to_string(max_val) + "]"
            );
        }
    }
    
    // Container validation
    template<typename Container>
    static void validate_non_empty_container(const Container& container, const std::string& field_name = "container") {
        if (container.empty()) {
            throw InputValidationException(field_name + " cannot be empty");
        }
    }
    
    template<typename Container>
    static void validate_container_size(const Container& container, size_t max_size, 
                                      const std::string& field_name = "container") {
        if (container.size() > max_size) {
            throw InputValidationException(
                field_name + " size " + std::to_string(container.size()) + 
                " exceeds maximum " + std::to_string(max_size)
            );
        }
    }
    
    // Key validation for maps
    template<typename ValueType>
    static void validate_key_exists(const std::map<std::string, ValueType>& map, const std::string& key, 
                                   const std::string& context = "map") {
        if (map.find(key) == map.end()) {
            throw InputValidationException("Key '" + key + "' not found in " + context);
        }
    }
    
    // Custom validation with predicate
    template<typename T, typename Predicate>
    static void validate_custom(const T& value, Predicate pred, const std::string& error_message) {
        if (!pred(value)) {
            throw InputValidationException(error_message);
        }
    }
};

// Safe casting utilities
class SafeCaster {
public:
    // Safe dynamic cast with exception on failure
    template<typename Target, typename Source>
    static Target* safe_cast(Source* source, const std::string& context = "") {
        if (!source) {
            throw UnsafeCastException("Cannot cast null pointer" + (context.empty() ? "" : " in " + context));
        }
        
        Target* result = dynamic_cast<Target*>(source);
        if (!result) {
            throw UnsafeCastException(
                "Failed to cast from " + std::string(typeid(Source).name()) + 
                " to " + std::string(typeid(Target).name()) +
                (context.empty() ? "" : " in " + context)
            );
        }
        return result;
    }
    
    // Safe dynamic cast for shared_ptr
    template<typename Target, typename Source>
    static std::shared_ptr<Target> safe_cast(const std::shared_ptr<Source>& source, const std::string& context = "") {
        if (!source) {
            throw UnsafeCastException("Cannot cast null shared_ptr" + (context.empty() ? "" : " in " + context));
        }
        
        auto result = std::dynamic_pointer_cast<Target>(source);
        if (!result) {
            throw UnsafeCastException(
                "Failed to cast shared_ptr from " + std::string(typeid(Source).name()) + 
                " to " + std::string(typeid(Target).name()) +
                (context.empty() ? "" : " in " + context)
            );
        }
        return result;
    }
    
    // Safe numeric cast with overflow checking
    template<typename Target, typename Source>
    static Target safe_numeric_cast(Source value, const std::string& context = "") 
        requires (std::is_arithmetic_v<Source> && std::is_arithmetic_v<Target>) {
        
        // Check for overflow/underflow
        if constexpr (std::is_integral_v<Source> && std::is_integral_v<Target>) {
            if (value > std::numeric_limits<Target>::max()) {
                throw UnsafeCastException(
                    "Numeric cast overflow: " + std::to_string(value) + 
                    " > " + std::to_string(std::numeric_limits<Target>::max()) +
                    (context.empty() ? "" : " in " + context)
                );
            }
            if (value < std::numeric_limits<Target>::lowest()) {
                throw UnsafeCastException(
                    "Numeric cast underflow: " + std::to_string(value) + 
                    " < " + std::to_string(std::numeric_limits<Target>::lowest()) +
                    (context.empty() ? "" : " in " + context)
                );
            }
        }
        
        return static_cast<Target>(value);
    }
    
    // Optional cast that returns nullptr on failure instead of throwing
    template<typename Target, typename Source>
    static Target* optional_cast(Source* source) noexcept {
        return source ? dynamic_cast<Target*>(source) : nullptr;
    }
    
    template<typename Target, typename Source>
    static std::shared_ptr<Target> optional_cast(const std::shared_ptr<Source>& source) noexcept {
        return source ? std::dynamic_pointer_cast<Target>(source) : nullptr;
    }
};

// Integer overflow protection utilities
class OverflowProtection {
public:
    // Safe addition
    template<typename T>
    static T safe_add(T a, T b, const std::string& context = "") 
        requires std::is_integral_v<T> {
        
        if constexpr (std::is_signed_v<T>) {
            if (b > 0 && a > std::numeric_limits<T>::max() - b) {
                throw IntegerOverflowException(
                    "Addition overflow: " + std::to_string(a) + " + " + std::to_string(b) +
                    (context.empty() ? "" : " in " + context)
                );
            }
            if (b < 0 && a < std::numeric_limits<T>::lowest() - b) {
                throw IntegerOverflowException(
                    "Addition underflow: " + std::to_string(a) + " + " + std::to_string(b) +
                    (context.empty() ? "" : " in " + context)
                );
            }
        } else {
            if (a > std::numeric_limits<T>::max() - b) {
                throw IntegerOverflowException(
                    "Addition overflow: " + std::to_string(a) + " + " + std::to_string(b) +
                    (context.empty() ? "" : " in " + context)
                );
            }
        }
        
        return a + b;
    }
    
    // Safe subtraction
    template<typename T>
    static T safe_subtract(T a, T b, const std::string& context = "") 
        requires std::is_integral_v<T> {
        
        if constexpr (std::is_signed_v<T>) {
            if (b < 0 && a > std::numeric_limits<T>::max() + b) {
                throw IntegerOverflowException(
                    "Subtraction overflow: " + std::to_string(a) + " - " + std::to_string(b) +
                    (context.empty() ? "" : " in " + context)
                );
            }
            if (b > 0 && a < std::numeric_limits<T>::lowest() + b) {
                throw IntegerOverflowException(
                    "Subtraction underflow: " + std::to_string(a) + " - " + std::to_string(b) +
                    (context.empty() ? "" : " in " + context)
                );
            }
        } else {
            if (a < b) {
                throw IntegerOverflowException(
                    "Subtraction underflow: " + std::to_string(a) + " - " + std::to_string(b) +
                    (context.empty() ? "" : " in " + context)
                );
            }
        }
        
        return a - b;
    }
    
    // Safe multiplication
    template<typename T>
    static T safe_multiply(T a, T b, const std::string& context = "") 
        requires std::is_integral_v<T> {
        
        if (a == 0 || b == 0) {
            return 0;
        }
        
        if constexpr (std::is_signed_v<T>) {
            if (a > 0) {
                if (b > 0 && a > std::numeric_limits<T>::max() / b) {
                    throw IntegerOverflowException(
                        "Multiplication overflow: " + std::to_string(a) + " * " + std::to_string(b) +
                        (context.empty() ? "" : " in " + context)
                    );
                }
                if (b < 0 && b < std::numeric_limits<T>::lowest() / a) {
                    throw IntegerOverflowException(
                        "Multiplication underflow: " + std::to_string(a) + " * " + std::to_string(b) +
                        (context.empty() ? "" : " in " + context)
                    );
                }
            } else {
                if (b > 0 && a < std::numeric_limits<T>::lowest() / b) {
                    throw IntegerOverflowException(
                        "Multiplication underflow: " + std::to_string(a) + " * " + std::to_string(b) +
                        (context.empty() ? "" : " in " + context)
                    );
                }
                if (b < 0 && a < std::numeric_limits<T>::max() / b) {
                    throw IntegerOverflowException(
                        "Multiplication overflow: " + std::to_string(a) + " * " + std::to_string(b) +
                        (context.empty() ? "" : " in " + context)
                    );
                }
            }
        } else {
            if (a > std::numeric_limits<T>::max() / b) {
                throw IntegerOverflowException(
                    "Multiplication overflow: " + std::to_string(a) + " * " + std::to_string(b) +
                    (context.empty() ? "" : " in " + context)
                );
            }
        }
        
        return a * b;
    }
    
    // Safe division for integral types
    template<typename T>
    static T safe_divide(T a, T b, const std::string& context = "") 
        requires std::is_integral_v<T> {
        
        if (b == 0) {
            throw IntegerOverflowException(
                "Division by zero: " + std::to_string(a) + " / " + std::to_string(b) +
                (context.empty() ? "" : " in " + context)
            );
        }
        
        if constexpr (std::is_signed_v<T>) {
            if (a == std::numeric_limits<T>::lowest() && b == -1) {
                throw IntegerOverflowException(
                    "Division overflow: " + std::to_string(a) + " / " + std::to_string(b) +
                    (context.empty() ? "" : " in " + context)
                );
            }
        }
        
        return a / b;
    }
    
    // Safe division for floating-point types
    template<typename T>
    static T safe_divide(T a, T b, const std::string& context = "") 
        requires std::is_floating_point_v<T> {
        
        if (b == T(0)) {
            throw IntegerOverflowException(
                "Division by zero: " + std::to_string(a) + " / " + std::to_string(b) +
                (context.empty() ? "" : " in " + context)
            );
        }
        
        T result = a / b;
        
        // Check for infinity or NaN
        if (!std::isfinite(result)) {
            throw IntegerOverflowException(
                "Division resulted in non-finite value: " + std::to_string(a) + " / " + std::to_string(b) +
                (context.empty() ? "" : " in " + context)
            );
        }
        
        return result;
    }
    
    // Safe increment
    template<typename T>
    static T safe_increment(T& value, const std::string& context = "") 
        requires std::is_integral_v<T> {
        
        if (value == std::numeric_limits<T>::max()) {
            throw IntegerOverflowException(
                "Increment overflow: " + std::to_string(value) +
                (context.empty() ? "" : " in " + context)
            );
        }
        
        return ++value;
    }
    
    // Safe decrement
    template<typename T>
    static T safe_decrement(T& value, const std::string& context = "") 
        requires std::is_integral_v<T> {
        
        if (value == std::numeric_limits<T>::lowest()) {
            throw IntegerOverflowException(
                "Decrement underflow: " + std::to_string(value) +
                (context.empty() ? "" : " in " + context)
            );
        }
        
        return --value;
    }
};

// Secure container access utilities
template<typename Container>
class SecureContainer {
private:
    Container& container_;
    std::string context_;
    
public:
    explicit SecureContainer(Container& container, std::string context = "") 
        : container_(container), context_(std::move(context)) {}
    
    // Safe element access
    auto& at(size_t index) {
        BoundsChecker::check_bounds(container_, index, context_);
        return container_[index];
    }
    
    const auto& at(size_t index) const {
        BoundsChecker::check_bounds(container_, index, context_);
        return container_[index];
    }
    
    // Safe front/back access
    auto& front() {
        if (container_.empty()) {
            throw BoundsCheckException("Cannot access front of empty container" + 
                                     (context_.empty() ? "" : " in " + context_));
        }
        return container_.front();
    }
    
    const auto& front() const {
        if (container_.empty()) {
            throw BoundsCheckException("Cannot access front of empty container" + 
                                     (context_.empty() ? "" : " in " + context_));
        }
        return container_.front();
    }
    
    auto& back() {
        if (container_.empty()) {
            throw BoundsCheckException("Cannot access back of empty container" + 
                                     (context_.empty() ? "" : " in " + context_));
        }
        return container_.back();
    }
    
    const auto& back() const {
        if (container_.empty()) {
            throw BoundsCheckException("Cannot access back of empty container" + 
                                     (context_.empty() ? "" : " in " + context_));
        }
        return container_.back();
    }
    
    // Safe pop operations
    void pop_back() {
        if (container_.empty()) {
            throw BoundsCheckException("Cannot pop from empty container" + 
                                     (context_.empty() ? "" : " in " + context_));
        }
        container_.pop_back();
    }
    
    // Delegate other operations
    auto size() const { return container_.size(); }
    auto empty() const { return container_.empty(); }
    auto begin() { return container_.begin(); }
    auto end() { return container_.end(); }
    auto begin() const { return container_.begin(); }
    auto end() const { return container_.end(); }
};

// Factory function for secure container access
template<typename Container>
SecureContainer<Container> make_secure(Container& container, const std::string& context = "") {
    return SecureContainer<Container>(container, context);
}

} // namespace Security

#endif // INPUT_VALIDATION_H