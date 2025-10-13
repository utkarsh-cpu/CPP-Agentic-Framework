#ifndef FRAMEWORKERROR_H
#define FRAMEWORKERROR_H

#include <exception>
#include <string>
#include <system_error>
#include <memory>
#include <vector>
#include <chrono>

namespace framework {

// Error categories for different types of errors
enum class ErrorCategory {
    LOGIC_ERROR,        // Programming errors, precondition violations
    RUNTIME_ERROR,      // Resource exhaustion, network failures
    SYSTEM_ERROR,       // OS-level errors, hardware failures
    VALIDATION_ERROR,   // Input validation failures
    CONCURRENCY_ERROR,  // Thread safety violations, deadlocks
    RESOURCE_ERROR,     // Memory allocation, file access failures
    TIMEOUT_ERROR,      // Operation timeout
    CANCELLATION_ERROR  // Operation cancelled
};

// Convert error category to string for logging/debugging
const char* to_string(ErrorCategory category) noexcept;

// Error context information for debugging and tracing
struct ErrorContext {
    std::string function_name;
    std::string file_name;
    int line_number = 0;
    std::chrono::system_clock::time_point timestamp;
    std::string additional_info;
    
    ErrorContext() : timestamp(std::chrono::system_clock::now()) {}
    
    ErrorContext(std::string func, std::string file, int line, std::string info = "")
        : function_name(std::move(func))
        , file_name(std::move(file))
        , line_number(line)
        , timestamp(std::chrono::system_clock::now())
        , additional_info(std::move(info)) {}
};

// Macro for creating error context with current location
#define FRAMEWORK_ERROR_CONTEXT(info) \
    framework::ErrorContext(__FUNCTION__, __FILE__, __LINE__, info)

// Structured error information class
class FrameworkError : public std::exception {
private:
    std::string message_;
    ErrorCategory category_;
    std::error_code error_code_;
    std::vector<ErrorContext> context_stack_;
    std::shared_ptr<FrameworkError> inner_error_;
    
public:
    // Basic constructor
    explicit FrameworkError(std::string message, 
                           ErrorCategory category = ErrorCategory::RUNTIME_ERROR)
        : message_(std::move(message))
        , category_(category) {}
    
    // Constructor with error code
    FrameworkError(std::string message, 
                   ErrorCategory category,
                   std::error_code error_code)
        : message_(std::move(message))
        , category_(category)
        , error_code_(error_code) {}
    
    // Constructor with context
    FrameworkError(std::string message,
                   ErrorCategory category,
                   ErrorContext context)
        : message_(std::move(message))
        , category_(category) {
        context_stack_.push_back(std::move(context));
    }
    
    // Constructor with error code and context
    FrameworkError(std::string message,
                   ErrorCategory category,
                   std::error_code error_code,
                   ErrorContext context)
        : message_(std::move(message))
        , category_(category)
        , error_code_(error_code) {
        context_stack_.push_back(std::move(context));
    }
    
    // Constructor with inner error for error chaining
    FrameworkError(std::string message,
                   ErrorCategory category,
                   std::shared_ptr<FrameworkError> inner_error)
        : message_(std::move(message))
        , category_(category)
        , inner_error_(std::move(inner_error)) {}
    
    // Standard exception interface
    const char* what() const noexcept override {
        return message_.c_str();
    }
    
    // Accessors
    const std::string& message() const noexcept { return message_; }
    ErrorCategory category() const noexcept { return category_; }
    const std::error_code& error_code() const noexcept { return error_code_; }
    const std::vector<ErrorContext>& context_stack() const noexcept { return context_stack_; }
    const std::shared_ptr<FrameworkError>& inner_error() const noexcept { return inner_error_; }
    
    // Add context to the error
    void add_context(ErrorContext context) {
        context_stack_.push_back(std::move(context));
    }
    
    // Check if error has specific category
    bool is_category(ErrorCategory cat) const noexcept {
        return category_ == cat;
    }
    
    // Get formatted error message with context
    std::string format_detailed() const;
    
    // Static factory methods for common error types
    static FrameworkError logic_error(const std::string& message, 
                                     const ErrorContext& context = {});
    static FrameworkError runtime_error(const std::string& message,
                                       const ErrorContext& context = {});
    static FrameworkError validation_error(const std::string& message,
                                          const ErrorContext& context = {});
    static FrameworkError timeout_error(const std::string& message,
                                       const ErrorContext& context = {});
    static FrameworkError resource_error(const std::string& message,
                                        const ErrorContext& context = {});
    static FrameworkError concurrency_error(const std::string& message,
                                           const ErrorContext& context = {});
};

} // namespace framework

#endif // FRAMEWORKERROR_H