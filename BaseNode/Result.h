#ifndef RESULT_H
#define RESULT_H

#include "FrameworkError.h"
#include <variant>
#include <type_traits>
#include <functional>

namespace framework {

// Forward declaration
template<typename T>
class Result;

// Helper type traits
template<typename T>
struct is_result : std::false_type {};

template<typename T>
struct is_result<Result<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_result_v = is_result<T>::value;

// Result type for operations that can fail
template<typename T>
class Result {
private:
    std::variant<T, FrameworkError> value_;
    
public:
    // Type aliases
    using value_type = T;
    using error_type = FrameworkError;
    
    // Constructors
    Result(T value) : value_(std::move(value)) {}
    Result(FrameworkError error) : value_(std::move(error)) {}
    
    // Copy and move constructors
    Result(const Result&) = default;
    Result(Result&&) = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) = default;
    
    // Check if result contains a value
    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(value_);
    }
    
    // Check if result contains an error
    [[nodiscard]] bool has_error() const noexcept {
        return std::holds_alternative<FrameworkError>(value_);
    }
    
    // Explicit bool conversion
    explicit operator bool() const noexcept {
        return has_value();
    }
    
    // Access the value (throws if error)
    [[nodiscard]] const T& value() const & {
        if (has_error()) {
            throw std::get<FrameworkError>(value_);
        }
        return std::get<T>(value_);
    }
    
    [[nodiscard]] T& value() & {
        if (has_error()) {
            throw std::get<FrameworkError>(value_);
        }
        return std::get<T>(value_);
    }
    
    [[nodiscard]] T&& value() && {
        if (has_error()) {
            throw std::get<FrameworkError>(value_);
        }
        return std::get<T>(std::move(value_));
    }
    
    // Access the error (throws if value)
    [[nodiscard]] const FrameworkError& error() const & {
        if (has_value()) {
            throw FrameworkError::logic_error("Result contains value, not error");
        }
        return std::get<FrameworkError>(value_);
    }
    
    [[nodiscard]] FrameworkError& error() & {
        if (has_value()) {
            throw FrameworkError::logic_error("Result contains value, not error");
        }
        return std::get<FrameworkError>(value_);
    }
    
    [[nodiscard]] FrameworkError&& error() && {
        if (has_value()) {
            throw FrameworkError::logic_error("Result contains value, not error");
        }
        return std::get<FrameworkError>(std::move(value_));
    }
    
    // Get value or default
    template<typename U>
    [[nodiscard]] T value_or(U&& default_value) const & {
        return has_value() ? std::get<T>(value_) : static_cast<T>(std::forward<U>(default_value));
    }
    
    template<typename U>
    [[nodiscard]] T value_or(U&& default_value) && {
        return has_value() ? std::get<T>(std::move(value_)) : static_cast<T>(std::forward<U>(default_value));
    }
    
    // Monadic operations
    
    // Map: transform the value if present, otherwise propagate error
    template<typename F>
    [[nodiscard]] auto map(F&& func) const & -> Result<std::invoke_result_t<F, const T&>> {
        using ReturnType = std::invoke_result_t<F, const T&>;
        
        if (has_error()) {
            return Result<ReturnType>(std::get<FrameworkError>(value_));
        }
        
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                std::invoke(std::forward<F>(func), std::get<T>(value_));
                return Result<ReturnType>(std::monostate{});
            } else {
                return Result<ReturnType>(std::invoke(std::forward<F>(func), std::get<T>(value_)));
            }
        } catch (const FrameworkError& e) {
            return Result<ReturnType>(e);
        } catch (const std::exception& e) {
            return Result<ReturnType>(FrameworkError::runtime_error(e.what()));
        }
    }
    
    template<typename F>
    [[nodiscard]] auto map(F&& func) && -> Result<std::invoke_result_t<F, T&&>> {
        using ReturnType = std::invoke_result_t<F, T&&>;
        
        if (has_error()) {
            return Result<ReturnType>(std::get<FrameworkError>(std::move(value_)));
        }
        
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                std::invoke(std::forward<F>(func), std::get<T>(std::move(value_)));
                return Result<ReturnType>(std::monostate{});
            } else {
                return Result<ReturnType>(std::invoke(std::forward<F>(func), std::get<T>(std::move(value_))));
            }
        } catch (const FrameworkError& e) {
            return Result<ReturnType>(e);
        } catch (const std::exception& e) {
            return Result<ReturnType>(FrameworkError::runtime_error(e.what()));
        }
    }
    
    // FlatMap: transform the value to another Result, flatten the result
    template<typename F>
    [[nodiscard]] auto flat_map(F&& func) const & -> std::invoke_result_t<F, const T&> {
        static_assert(is_result_v<std::invoke_result_t<F, const T&>>, 
                     "Function must return a Result type");
        
        if (has_error()) {
            return std::invoke_result_t<F, const T&>(std::get<FrameworkError>(value_));
        }
        
        try {
            return std::invoke(std::forward<F>(func), std::get<T>(value_));
        } catch (const FrameworkError& e) {
            return std::invoke_result_t<F, const T&>(e);
        } catch (const std::exception& e) {
            return std::invoke_result_t<F, const T&>(FrameworkError::runtime_error(e.what()));
        }
    }
    
    template<typename F>
    [[nodiscard]] auto flat_map(F&& func) && -> std::invoke_result_t<F, T&&> {
        static_assert(is_result_v<std::invoke_result_t<F, T&&>>, 
                     "Function must return a Result type");
        
        if (has_error()) {
            return std::invoke_result_t<F, T&&>(std::get<FrameworkError>(std::move(value_)));
        }
        
        try {
            return std::invoke(std::forward<F>(func), std::get<T>(std::move(value_)));
        } catch (const FrameworkError& e) {
            return std::invoke_result_t<F, T&&>(e);
        } catch (const std::exception& e) {
            return std::invoke_result_t<F, T&&>(FrameworkError::runtime_error(e.what()));
        }
    }
    
    // Map error: transform the error if present, otherwise propagate value
    template<typename F>
    [[nodiscard]] Result map_error(F&& func) const & {
        if (has_value()) {
            return Result(std::get<T>(value_));
        }
        
        try {
            return Result(std::invoke(std::forward<F>(func), std::get<FrameworkError>(value_)));
        } catch (const FrameworkError& e) {
            return Result(e);
        } catch (const std::exception& e) {
            return Result(FrameworkError::runtime_error(e.what()));
        }
    }
    
    template<typename F>
    [[nodiscard]] Result map_error(F&& func) && {
        if (has_value()) {
            return Result(std::get<T>(std::move(value_)));
        }
        
        try {
            return Result(std::invoke(std::forward<F>(func), std::get<FrameworkError>(std::move(value_))));
        } catch (const FrameworkError& e) {
            return Result(e);
        } catch (const std::exception& e) {
            return Result(FrameworkError::runtime_error(e.what()));
        }
    }
    
    // Execute function on value if present, return self
    template<typename F>
    const Result& inspect(F&& func) const & {
        if (has_value()) {
            std::invoke(std::forward<F>(func), std::get<T>(value_));
        }
        return *this;
    }
    
    template<typename F>
    Result&& inspect(F&& func) && {
        if (has_value()) {
            std::invoke(std::forward<F>(func), std::get<T>(value_));
        }
        return std::move(*this);
    }
    
    // Execute function on error if present, return self
    template<typename F>
    const Result& inspect_error(F&& func) const & {
        if (has_error()) {
            std::invoke(std::forward<F>(func), std::get<FrameworkError>(value_));
        }
        return *this;
    }
    
    template<typename F>
    Result&& inspect_error(F&& func) && {
        if (has_error()) {
            std::invoke(std::forward<F>(func), std::get<FrameworkError>(value_));
        }
        return std::move(*this);
    }
};

// Specialization for void type
template<>
class Result<void> {
private:
    std::variant<std::monostate, FrameworkError> value_;
    
public:
    using value_type = void;
    using error_type = FrameworkError;
    
    // Constructors
    Result() : value_(std::monostate{}) {}
    Result(FrameworkError error) : value_(std::move(error)) {}
    
    // Check if result is successful
    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<std::monostate>(value_);
    }
    
    [[nodiscard]] bool has_error() const noexcept {
        return std::holds_alternative<FrameworkError>(value_);
    }
    
    explicit operator bool() const noexcept {
        return has_value();
    }
    
    // Throw if error
    void value() const {
        if (has_error()) {
            throw std::get<FrameworkError>(value_);
        }
    }
    
    // Access the error
    [[nodiscard]] const FrameworkError& error() const & {
        if (has_value()) {
            throw FrameworkError::logic_error("Result contains value, not error");
        }
        return std::get<FrameworkError>(value_);
    }
    
    [[nodiscard]] FrameworkError&& error() && {
        if (has_value()) {
            throw FrameworkError::logic_error("Result contains value, not error");
        }
        return std::get<FrameworkError>(std::move(value_));
    }
};

// Helper functions for creating Results
template<typename T>
[[nodiscard]] Result<std::decay_t<T>> make_result(T&& value) {
    return Result<std::decay_t<T>>(std::forward<T>(value));
}

template<typename T>
[[nodiscard]] Result<T> make_error_result(FrameworkError error) {
    return Result<T>(std::move(error));
}

[[nodiscard]] inline Result<void> make_success() {
    return Result<void>();
}

[[nodiscard]] inline Result<void> make_error(FrameworkError error) {
    return Result<void>(std::move(error));
}

} // namespace framework

#endif // RESULT_H