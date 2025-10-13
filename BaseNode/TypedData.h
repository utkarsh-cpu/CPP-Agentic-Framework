#ifndef TYPEDDATA_H
#define TYPEDDATA_H

#include "FrameworkData.h"
#include <type_traits>
#include <functional>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <memory>
#include <concepts>

// Forward declarations
template<typename T>
class TypedData;

template<typename T>
class ImmutableTypedData;

// Concept for serializable types
template<typename T>
concept Serializable = requires(const T& t, const std::string& s) {
    { std::to_string(t) } -> std::convertible_to<std::string>;
} || std::is_arithmetic_v<T> || std::is_same_v<T, std::string>;

// Concept for types that can be validated
template<typename T>
concept Validatable = requires(const T& t) {
    { t == t } -> std::convertible_to<bool>;
};

// Validation function type
template<typename T>
using ValidationFunction = std::function<bool(const T&)>;

// Constraint checking system
template<typename T>
class Constraint {
public:
    explicit Constraint(ValidationFunction<T> validator, std::string description = "")
        : validator_(std::move(validator)), description_(std::move(description)) {}
    
    bool validate(const T& value) const {
        return validator_(value);
    }
    
    const std::string& description() const noexcept {
        return description_;
    }

private:
    ValidationFunction<T> validator_;
    std::string description_;
};

// Common constraint factory functions
template<typename T>
class Constraints {
public:
    // Range constraints for comparable types
    template<typename U = T>
    static Constraint<T> range(const U& min, const U& max) 
        requires std::totally_ordered<U> {
        return Constraint<T>(
            [min, max](const T& value) { return value >= min && value <= max; },
            "Value must be between " + std::to_string(min) + " and " + std::to_string(max)
        );
    }
    
    // Minimum value constraint
    template<typename U = T>
    static Constraint<T> min(const U& minimum) 
        requires std::totally_ordered<U> {
        return Constraint<T>(
            [minimum](const T& value) { return value >= minimum; },
            "Value must be >= " + std::to_string(minimum)
        );
    }
    
    // Maximum value constraint
    template<typename U = T>
    static Constraint<T> max(const U& maximum) 
        requires std::totally_ordered<U> {
        return Constraint<T>(
            [maximum](const T& value) { return value <= maximum; },
            "Value must be <= " + std::to_string(maximum)
        );
    }
    
    // Non-empty constraint for containers
    template<typename U = T>
    static Constraint<T> not_empty() 
        requires requires(const U& u) { u.empty(); } {
        return Constraint<T>(
            [](const T& value) { return !value.empty(); },
            "Value must not be empty"
        );
    }
    
    // Custom constraint
    static Constraint<T> custom(ValidationFunction<T> validator, std::string description = "") {
        return Constraint<T>(std::move(validator), std::move(description));
    }
};

// Type-safe data container with validation and constraints
template<typename T>
class TypedData : public FrameworkData {
private:
    T data_;
    std::vector<Constraint<T>> constraints_;
    
public:
    using value_type = T;
    
    // Constructors
    explicit TypedData(T data) : data_(std::move(data)) {
        validate_constraints();
    }
    
    TypedData(T data, std::vector<Constraint<T>> constraints) 
        : data_(std::move(data)), constraints_(std::move(constraints)) {
        validate_constraints();
    }
    
    // Copy constructor
    TypedData(const TypedData& other) 
        : data_(other.data_), constraints_(other.constraints_) {}
    
    // Move constructor
    TypedData(TypedData&& other) noexcept 
        : data_(std::move(other.data_)), constraints_(std::move(other.constraints_)) {}
    
    // Copy assignment
    TypedData& operator=(const TypedData& other) {
        if (this != &other) {
            data_ = other.data_;
            constraints_ = other.constraints_;
            validate_constraints();
        }
        return *this;
    }
    
    // Move assignment
    TypedData& operator=(TypedData&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            constraints_ = std::move(other.constraints_);
        }
        return *this;
    }
    
    // Value access
    [[nodiscard]] const T& get() const noexcept { 
        return data_; 
    }
    
    [[nodiscard]] T& get() noexcept { 
        return data_; 
    }
    
    // Set new value with validation
    void set(T new_value) {
        T temp = std::move(new_value);
        validate_value(temp);
        data_ = std::move(temp);
    }
    
    // Add constraint
    void add_constraint(Constraint<T> constraint) {
        validate_value(data_, constraint);
        constraints_.emplace_back(std::move(constraint));
    }
    
    // Remove all constraints
    void clear_constraints() {
        constraints_.clear();
    }
    
    // Check if value satisfies all constraints
    [[nodiscard]] bool is_valid() const noexcept {
        try {
            validate_constraints();
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // Get validation errors
    [[nodiscard]] std::vector<std::string> get_validation_errors() const {
        std::vector<std::string> errors;
        for (const auto& constraint : constraints_) {
            if (!constraint.validate(data_)) {
                errors.push_back(constraint.description());
            }
        }
        return errors;
    }
    
    // FrameworkData interface implementation
    [[nodiscard]] std::unique_ptr<FrameworkData> clone() const override {
        return std::make_unique<TypedData<T>>(*this);
    }
    
    [[nodiscard]] const std::type_info& type() const noexcept override {
        return typeid(TypedData<T>);
    }
    
    // Serialization support
    std::string serialize() const override {
        if constexpr (Serializable<T>) {
            return serialize_impl();
        } else {
            return ""; // Unsupported type
        }
    }
    
    bool deserialize(std::string_view data) override {
        if constexpr (Serializable<T>) {
            return deserialize_impl(std::string{data});
        } else {
            return false; // Unsupported type
        }
    }
    
    // Equality operators
    bool operator==(const TypedData<T>& other) const noexcept 
        requires std::equality_comparable<T> {
        return data_ == other.data_;
    }
    
    bool operator!=(const TypedData<T>& other) const noexcept 
        requires std::equality_comparable<T> {
        return !(*this == other);
    }
    
    // Comparison operators
    bool operator<(const TypedData<T>& other) const noexcept 
        requires std::totally_ordered<T> {
        return data_ < other.data_;
    }
    
    bool operator<=(const TypedData<T>& other) const noexcept 
        requires std::totally_ordered<T> {
        return data_ <= other.data_;
    }
    
    bool operator>(const TypedData<T>& other) const noexcept 
        requires std::totally_ordered<T> {
        return data_ > other.data_;
    }
    
    bool operator>=(const TypedData<T>& other) const noexcept 
        requires std::totally_ordered<T> {
        return data_ >= other.data_;
    }

private:
    void validate_constraints() const {
        for (const auto& constraint : constraints_) {
            if (!constraint.validate(data_)) {
                throw std::invalid_argument("Constraint violation: " + constraint.description());
            }
        }
    }
    
    void validate_value(const T& value) const {
        for (const auto& constraint : constraints_) {
            validate_value(value, constraint);
        }
    }
    
    void validate_value(const T& value, const Constraint<T>& constraint) const {
        if (!constraint.validate(value)) {
            throw std::invalid_argument("Constraint violation: " + constraint.description());
        }
    }
    
    // Serialization implementation for different types
    std::string serialize_impl() const {
        if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(data_);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return data_;
        } else {
            // For other types, try to use their own serialization if available
            if constexpr (requires { data_.serialize(); }) {
                return data_.serialize();
            } else {
                return "";
            }
        }
    }
    
    bool deserialize_impl(const std::string& data) {
        try {
            if constexpr (std::is_same_v<T, std::string>) {
                set(data);
                return true;
            } else if constexpr (std::is_same_v<T, int>) {
                set(std::stoi(data));
                return true;
            } else if constexpr (std::is_same_v<T, long>) {
                set(std::stol(data));
                return true;
            } else if constexpr (std::is_same_v<T, long long>) {
                set(std::stoll(data));
                return true;
            } else if constexpr (std::is_same_v<T, float>) {
                set(std::stof(data));
                return true;
            } else if constexpr (std::is_same_v<T, double>) {
                set(std::stod(data));
                return true;
            } else if constexpr (std::is_same_v<T, long double>) {
                set(std::stold(data));
                return true;
            } else {
                // For other types, try to use their own deserialization if available
                if constexpr (requires { data_.deserialize(data); }) {
                    T temp = data_;
                    if (temp.deserialize(data)) {
                        set(std::move(temp));
                        return true;
                    }
                }
                return false;
            }
        } catch (...) {
            return false;
        }
    }
};
// Immutable version of TypedData
template<typename T>
class ImmutableTypedData : public FrameworkData {
private:
    const T data_;
    const std::vector<Constraint<T>> constraints_;
    
public:
    using value_type = T;
    
    // Constructors
    explicit ImmutableTypedData(T data) : data_(std::move(data)) {
        validate_constraints();
    }
    
    ImmutableTypedData(T data, std::vector<Constraint<T>> constraints) 
        : data_(std::move(data)), constraints_(std::move(constraints)) {
        validate_constraints();
    }
    
    // Copy constructor
    ImmutableTypedData(const ImmutableTypedData& other) 
        : data_(other.data_), constraints_(other.constraints_) {}
    
    // Move constructor
    ImmutableTypedData(ImmutableTypedData&& other) noexcept 
        : data_(std::move(other.data_)), constraints_(std::move(other.constraints_)) {}
    
    // No assignment operators for immutable data
    ImmutableTypedData& operator=(const ImmutableTypedData&) = delete;
    ImmutableTypedData& operator=(ImmutableTypedData&&) = delete;
    
    // Value access (read-only)
    [[nodiscard]] const T& get() const noexcept { 
        return data_; 
    }
    
    // Create new instance with different value
    [[nodiscard]] ImmutableTypedData<T> with_value(T new_value) const {
        return ImmutableTypedData<T>(std::move(new_value), constraints_);
    }
    
    // Create new instance with additional constraint
    [[nodiscard]] ImmutableTypedData<T> with_constraint(Constraint<T> constraint) const {
        auto new_constraints = constraints_;
        // Validate the current value against the new constraint
        if (!constraint.validate(data_)) {
            throw std::invalid_argument("Current value violates new constraint: " + constraint.description());
        }
        new_constraints.emplace_back(std::move(constraint));
        return ImmutableTypedData<T>(data_, std::move(new_constraints));
    }
    
    // Check if value satisfies all constraints
    [[nodiscard]] bool is_valid() const noexcept {
        try {
            validate_constraints();
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // Get validation errors
    [[nodiscard]] std::vector<std::string> get_validation_errors() const {
        std::vector<std::string> errors;
        for (const auto& constraint : constraints_) {
            if (!constraint.validate(data_)) {
                errors.push_back(constraint.description());
            }
        }
        return errors;
    }
    
    // FrameworkData interface implementation
    [[nodiscard]] std::unique_ptr<FrameworkData> clone() const override {
        return std::make_unique<ImmutableTypedData<T>>(*this);
    }
    
    [[nodiscard]] const std::type_info& type() const noexcept override {
        return typeid(ImmutableTypedData<T>);
    }
    
    // Serialization support
    std::string serialize() const override {
        if constexpr (Serializable<T>) {
            return serialize_impl();
        } else {
            return ""; // Unsupported type
        }
    }
    
    bool deserialize(std::string_view data) override {
        // Immutable data cannot be deserialized in-place
        (void)data;
        return false;
    }
    
    // Create new instance from serialized data
    [[nodiscard]] static std::unique_ptr<ImmutableTypedData<T>> from_serialized(
        std::string_view data, 
        const std::vector<Constraint<T>>& constraints = {}) {
        
        if constexpr (Serializable<T>) {
            try {
                T value = deserialize_value(std::string{data});
                return std::make_unique<ImmutableTypedData<T>>(std::move(value), constraints);
            } catch (...) {
                return nullptr;
            }
        } else {
            return nullptr;
        }
    }
    
    // Equality operators
    bool operator==(const ImmutableTypedData<T>& other) const noexcept 
        requires std::equality_comparable<T> {
        return data_ == other.data_;
    }
    
    bool operator!=(const ImmutableTypedData<T>& other) const noexcept 
        requires std::equality_comparable<T> {
        return !(*this == other);
    }
    
    // Comparison operators
    bool operator<(const ImmutableTypedData<T>& other) const noexcept 
        requires std::totally_ordered<T> {
        return data_ < other.data_;
    }
    
    bool operator<=(const ImmutableTypedData<T>& other) const noexcept 
        requires std::totally_ordered<T> {
        return data_ <= other.data_;
    }
    
    bool operator>(const ImmutableTypedData<T>& other) const noexcept 
        requires std::totally_ordered<T> {
        return data_ > other.data_;
    }
    
    bool operator>=(const ImmutableTypedData<T>& other) const noexcept 
        requires std::totally_ordered<T> {
        return data_ >= other.data_;
    }

private:
    void validate_constraints() const {
        for (const auto& constraint : constraints_) {
            if (!constraint.validate(data_)) {
                throw std::invalid_argument("Constraint violation: " + constraint.description());
            }
        }
    }
    
    // Serialization implementation for different types
    std::string serialize_impl() const {
        if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(data_);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return data_;
        } else {
            // For other types, try to use their own serialization if available
            if constexpr (requires { data_.serialize(); }) {
                return data_.serialize();
            } else {
                return "";
            }
        }
    }
    
    static T deserialize_value(const std::string& data) {
        if constexpr (std::is_same_v<T, std::string>) {
            return data;
        } else if constexpr (std::is_same_v<T, int>) {
            return std::stoi(data);
        } else if constexpr (std::is_same_v<T, long>) {
            return std::stol(data);
        } else if constexpr (std::is_same_v<T, long long>) {
            return std::stoll(data);
        } else if constexpr (std::is_same_v<T, float>) {
            return std::stof(data);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::stod(data);
        } else if constexpr (std::is_same_v<T, long double>) {
            return std::stold(data);
        } else {
            throw std::invalid_argument("Unsupported type for deserialization");
        }
    }
};

// Factory functions for easier creation
template<typename T>
[[nodiscard]] std::unique_ptr<TypedData<T>> make_typed_data(T value) {
    return std::make_unique<TypedData<T>>(std::move(value));
}

template<typename T>
[[nodiscard]] std::unique_ptr<TypedData<T>> make_typed_data(T value, std::vector<Constraint<T>> constraints) {
    return std::make_unique<TypedData<T>>(std::move(value), std::move(constraints));
}

template<typename T>
[[nodiscard]] std::unique_ptr<ImmutableTypedData<T>> make_immutable_typed_data(T value) {
    return std::make_unique<ImmutableTypedData<T>>(std::move(value));
}

template<typename T>
[[nodiscard]] std::unique_ptr<ImmutableTypedData<T>> make_immutable_typed_data(T value, std::vector<Constraint<T>> constraints) {
    return std::make_unique<ImmutableTypedData<T>>(std::move(value), std::move(constraints));
}

// Type aliases for common types
using IntData = TypedData<int>;
using DoubleData = TypedData<double>;
using StringTypedData = TypedData<std::string>;
using BoolData = TypedData<bool>;

using ImmutableIntData = ImmutableTypedData<int>;
using ImmutableDoubleData = ImmutableTypedData<double>;
using ImmutableStringTypedData = ImmutableTypedData<std::string>;
using ImmutableBoolData = ImmutableTypedData<bool>;

#endif //TYPEDDATA_H