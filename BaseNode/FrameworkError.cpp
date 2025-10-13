#include "FrameworkError.h"
#include <sstream>
#include <iomanip>

namespace framework {

const char* to_string(ErrorCategory category) noexcept {
    switch (category) {
        case ErrorCategory::LOGIC_ERROR:        return "LOGIC_ERROR";
        case ErrorCategory::RUNTIME_ERROR:      return "RUNTIME_ERROR";
        case ErrorCategory::SYSTEM_ERROR:       return "SYSTEM_ERROR";
        case ErrorCategory::VALIDATION_ERROR:   return "VALIDATION_ERROR";
        case ErrorCategory::CONCURRENCY_ERROR:  return "CONCURRENCY_ERROR";
        case ErrorCategory::RESOURCE_ERROR:     return "RESOURCE_ERROR";
        case ErrorCategory::TIMEOUT_ERROR:      return "TIMEOUT_ERROR";
        case ErrorCategory::CANCELLATION_ERROR: return "CANCELLATION_ERROR";
        default:                                return "UNKNOWN_ERROR";
    }
}

std::string FrameworkError::format_detailed() const {
    std::ostringstream oss;
    
    // Main error information
    oss << "[" << to_string(category_) << "] " << message_;
    
    // Add error code if present
    if (error_code_) {
        oss << " (Error Code: " << error_code_.value() 
            << " - " << error_code_.message() << ")";
    }
    
    // Add context stack
    if (!context_stack_.empty()) {
        oss << "\nContext Stack:\n";
        for (size_t i = 0; i < context_stack_.size(); ++i) {
            const auto& ctx = context_stack_[i];
            oss << "  [" << i << "] " << ctx.function_name;
            
            if (!ctx.file_name.empty()) {
                // Extract just the filename from the full path
                size_t last_slash = ctx.file_name.find_last_of("/\\");
                std::string filename = (last_slash != std::string::npos) 
                    ? ctx.file_name.substr(last_slash + 1) 
                    : ctx.file_name;
                oss << " (" << filename << ":" << ctx.line_number << ")";
            }
            
            if (!ctx.additional_info.empty()) {
                oss << " - " << ctx.additional_info;
            }
            
            // Format timestamp
            auto time_t = std::chrono::system_clock::to_time_t(ctx.timestamp);
            oss << " [" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "]";
            oss << "\n";
        }
    }
    
    // Add inner error if present
    if (inner_error_) {
        oss << "\nInner Error:\n" << inner_error_->format_detailed();
    }
    
    return oss.str();
}

// Static factory methods
FrameworkError FrameworkError::logic_error(const std::string& message, 
                                          const ErrorContext& context) {
    FrameworkError error(message, ErrorCategory::LOGIC_ERROR);
    if (!context.function_name.empty()) {
        error.add_context(context);
    }
    return error;
}

FrameworkError FrameworkError::runtime_error(const std::string& message,
                                            const ErrorContext& context) {
    FrameworkError error(message, ErrorCategory::RUNTIME_ERROR);
    if (!context.function_name.empty()) {
        error.add_context(context);
    }
    return error;
}

FrameworkError FrameworkError::validation_error(const std::string& message,
                                               const ErrorContext& context) {
    FrameworkError error(message, ErrorCategory::VALIDATION_ERROR);
    if (!context.function_name.empty()) {
        error.add_context(context);
    }
    return error;
}

FrameworkError FrameworkError::timeout_error(const std::string& message,
                                            const ErrorContext& context) {
    FrameworkError error(message, ErrorCategory::TIMEOUT_ERROR);
    if (!context.function_name.empty()) {
        error.add_context(context);
    }
    return error;
}

FrameworkError FrameworkError::resource_error(const std::string& message,
                                             const ErrorContext& context) {
    FrameworkError error(message, ErrorCategory::RESOURCE_ERROR);
    if (!context.function_name.empty()) {
        error.add_context(context);
    }
    return error;
}

FrameworkError FrameworkError::concurrency_error(const std::string& message,
                                                const ErrorContext& context) {
    FrameworkError error(message, ErrorCategory::CONCURRENCY_ERROR);
    if (!context.function_name.empty()) {
        error.add_context(context);
    }
    return error;
}

} // namespace framework