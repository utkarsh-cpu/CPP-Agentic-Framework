#ifndef SECURITY_UTILS_H
#define SECURITY_UTILS_H

#include "InputValidation.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <type_traits>
#include <limits>

namespace Security {

// Secure string operations
class SecureString {
public:
    // Safe substring with bounds checking
    static std::string safe_substr(const std::string& str, size_t pos, size_t len = std::string::npos) {
        BoundsChecker::check_string_bounds(str, pos, len, "safe_substr");
        return str.substr(pos, len);
    }
    
    // Safe string indexing
    static char safe_at(const std::string& str, size_t index) {
        BoundsChecker::check_bounds(str, index, "string indexing");
        return str[index];
    }
    
    // Safe string concatenation with length limits
    static std::string safe_concat(const std::string& a, const std::string& b, size_t max_length = 10000) {
        size_t total_length = OverflowProtection::safe_add(a.length(), b.length(), "string concatenation");
        if (total_length > max_length) {
            throw InputValidationException(
                "Concatenated string length " + std::to_string(total_length) + 
                " exceeds maximum " + std::to_string(max_length)
            );
        }
        return a + b;
    }
    
    // Safe string replacement with bounds checking
    static std::string safe_replace(const std::string& str, size_t pos, size_t len, const std::string& replacement) {
        BoundsChecker::check_string_bounds(str, pos, len, "string replacement");
        std::string result = str;
        result.replace(pos, len, replacement);
        return result;
    }
};

// Secure memory operations
class SecureMemory {
public:
    // Safe memory allocation with size limits
    template<typename T>
    static std::unique_ptr<T[]> safe_allocate_array(size_t count, size_t max_count = 1000000) {
        InputValidator::validate_positive(count, "allocation count");
        InputValidator::validate_range(count, size_t(1), max_count, "allocation count");
        
        // Check for potential overflow in byte calculation
        size_t total_bytes = OverflowProtection::safe_multiply(count, sizeof(T), "memory allocation");
        
        // Additional check for reasonable memory usage (e.g., 1GB limit)
        constexpr size_t MAX_BYTES = 1024 * 1024 * 1024; // 1GB
        if (total_bytes > MAX_BYTES) {
            throw SecurityException(
                "Memory allocation of " + std::to_string(total_bytes) + 
                " bytes exceeds maximum " + std::to_string(MAX_BYTES) + " bytes"
            );
        }
        
        return std::make_unique<T[]>(count);
    }
    
    // Safe memory copy with bounds checking
    template<typename T>
    static void safe_copy(const T* source, T* dest, size_t count, size_t dest_capacity) {
        BoundsChecker::check_not_null(source, "source pointer");
        BoundsChecker::check_not_null(dest, "destination pointer");
        InputValidator::validate_positive(count, "copy count");
        
        if (count > dest_capacity) {
            throw BoundsCheckException(
                "Copy count " + std::to_string(count) + 
                " exceeds destination capacity " + std::to_string(dest_capacity)
            );
        }
        
        std::copy(source, source + count, dest);
    }
};

// Secure container operations
template<typename Container>
class SecureContainerOps {
public:
    // Safe container resize with limits
    static void safe_resize(Container& container, size_t new_size, size_t max_size = 1000000) {
        InputValidator::validate_range(new_size, size_t(0), max_size, "container resize");
        container.resize(new_size);
    }
    
    // Safe container reserve with limits
    static void safe_reserve(Container& container, size_t capacity, size_t max_capacity = 1000000) {
        InputValidator::validate_range(capacity, size_t(0), max_capacity, "container reserve");
        container.reserve(capacity);
    }
    
    // Safe element insertion with size checking
    template<typename T>
    static void safe_insert(Container& container, T&& element, size_t max_size = 1000000) {
        if (container.size() >= max_size) {
            throw SecurityException(
                "Container size " + std::to_string(container.size()) + 
                " would exceed maximum " + std::to_string(max_size) + " after insertion"
            );
        }
        container.emplace_back(std::forward<T>(element));
    }
};

// Secure file path operations
class SecureFilePath {
public:
    // Validate file path for security issues
    static void validate_path(const std::string& path) {
        InputValidator::validate_non_empty_string(path, "file path");
        InputValidator::validate_string_length(path, 4096, "file path"); // Max path length
        
        // Check for path traversal attempts
        if (path.find("..") != std::string::npos) {
            throw SecurityException("Path traversal detected in: " + path);
        }
        
        // Check for null bytes (can be used to bypass security checks)
        if (path.find('\0') != std::string::npos) {
            throw SecurityException("Null byte detected in path: " + path);
        }
        
        // Check for suspicious characters
        const std::string dangerous_chars = "<>:\"|?*";
        for (char c : dangerous_chars) {
            if (path.find(c) != std::string::npos) {
                throw SecurityException("Dangerous character '" + std::string(1, c) + "' in path: " + path);
            }
        }
    }
    
    // Normalize path to prevent bypasses
    static std::string normalize_path(const std::string& path) {
        validate_path(path);
        
        // Basic normalization - in production, use a proper path library
        std::string normalized = path;
        
        // Replace backslashes with forward slashes
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        
        // Remove duplicate slashes
        size_t pos = 0;
        while ((pos = normalized.find("//", pos)) != std::string::npos) {
            normalized.replace(pos, 2, "/");
        }
        
        return normalized;
    }
};

// Secure numeric operations with additional safety checks
class SecureNumeric {
public:
    // Safe division with additional checks
    template<typename T>
    static T safe_divide_with_precision(T numerator, T denominator, T min_result = std::numeric_limits<T>::lowest(),
                                       T max_result = std::numeric_limits<T>::max()) 
        requires std::is_arithmetic_v<T> {
        
        T result = OverflowProtection::safe_divide(numerator, denominator, "secure division");
        
        InputValidator::validate_range(result, min_result, max_result, "division result");
        
        return result;
    }
    
    // Safe modulo operation
    template<typename T>
    static T safe_modulo(T dividend, T divisor) 
        requires std::is_integral_v<T> {
        
        if (divisor == 0) {
            throw IntegerOverflowException("Modulo by zero");
        }
        
        if constexpr (std::is_signed_v<T>) {
            if (dividend == std::numeric_limits<T>::lowest() && divisor == -1) {
                throw IntegerOverflowException("Modulo overflow");
            }
        }
        
        return dividend % divisor;
    }
    
    // Safe power operation with overflow checking
    template<typename T>
    static T safe_power(T base, T exponent) 
        requires std::is_integral_v<T> {
        
        InputValidator::validate_non_negative(exponent, "exponent");
        InputValidator::validate_range(exponent, T(0), T(63), "exponent"); // Reasonable limit
        
        if (base == 0) {
            return exponent == 0 ? 1 : 0;
        }
        
        if (base == 1 || exponent == 0) {
            return 1;
        }
        
        T result = 1;
        T current_base = base;
        T current_exp = exponent;
        
        while (current_exp > 0) {
            if (current_exp % 2 == 1) {
                result = OverflowProtection::safe_multiply(result, current_base, "power calculation");
            }
            if (current_exp > 1) {
                current_base = OverflowProtection::safe_multiply(current_base, current_base, "power calculation");
            }
            current_exp /= 2;
        }
        
        return result;
    }
};

// Security audit logging
class SecurityAudit {
private:
    static std::vector<std::string> audit_log_;
    
public:
    // Log security events
    static void log_security_event(const std::string& event, const std::string& context = "") {
        std::string log_entry = "SECURITY: " + event;
        if (!context.empty()) {
            log_entry += " [" + context + "]";
        }
        
        // In production, this would write to a secure log file
        audit_log_.push_back(log_entry);
        
        // Also output to stderr for immediate visibility
        std::cerr << log_entry << std::endl;
    }
    
    // Get audit log (for testing/debugging)
    static const std::vector<std::string>& get_audit_log() {
        return audit_log_;
    }
    
    // Clear audit log (for testing)
    static void clear_audit_log() {
        audit_log_.clear();
    }
};

// Initialize static member
inline std::vector<std::string> SecurityAudit::audit_log_;

// RAII security context for tracking operations
class SecurityContext {
private:
    std::string operation_name_;
    
public:
    explicit SecurityContext(std::string operation_name) 
        : operation_name_(std::move(operation_name)) {
        SecurityAudit::log_security_event("Starting operation: " + operation_name_);
    }
    
    ~SecurityContext() {
        SecurityAudit::log_security_event("Completed operation: " + operation_name_);
    }
    
    // Non-copyable, non-movable
    SecurityContext(const SecurityContext&) = delete;
    SecurityContext& operator=(const SecurityContext&) = delete;
    SecurityContext(SecurityContext&&) = delete;
    SecurityContext& operator=(SecurityContext&&) = delete;
};

// Macro for easy security context creation
#define SECURITY_CONTEXT(name) Security::SecurityContext _sec_ctx(name)

} // namespace Security

#endif // SECURITY_UTILS_H