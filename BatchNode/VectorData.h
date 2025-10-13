#ifndef VECTORDATA_H
#define VECTORDATA_H

#include "../BaseNode/FrameworkData.h"
#include "../BaseNode/InputValidation.h"
#include <vector>
#include <memory>
#include <typeinfo>
#include <sstream>
#include <stdexcept>

struct VectorData : public FrameworkData {
    std::vector<std::shared_ptr<FrameworkData>> value;
    
    // Constructors
    VectorData() = default;
    explicit VectorData(std::vector<std::shared_ptr<FrameworkData>> val) 
        : value(std::move(val)) {}
    
    // Perfect forwarding constructor for initializer list
    template<typename... Args>
    explicit VectorData(Args&&... args) : value{std::forward<Args>(args)...} {}
    
    // Copy constructor with deep copy
    VectorData(const VectorData& other) {
        value.reserve(other.value.size());
        for (const auto& item : other.value) {
            if (item) {
                value.push_back(item->clone());
            } else {
                value.push_back(nullptr);
            }
        }
    }
    
    // Copy assignment with deep copy
    VectorData& operator=(const VectorData& other) {
        if (this != &other) {
            value.clear();
            value.reserve(other.value.size());
            for (const auto& item : other.value) {
                if (item) {
                    value.push_back(item->clone());
                } else {
                    value.push_back(nullptr);
                }
            }
        }
        return *this;
    }
    
    // Move constructor and assignment
    VectorData(VectorData&& other) noexcept : value(std::move(other.value)) {}
    VectorData& operator=(VectorData&& other) noexcept {
        if (this != &other) {
            value = std::move(other.value);
        }
        return *this;
    }
    
    // FrameworkData interface implementation
    [[nodiscard]] std::unique_ptr<FrameworkData> clone() const override {
        return std::make_unique<VectorData>(*this);
    }
    
    [[nodiscard]] const std::type_info& type() const noexcept override {
        return typeid(VectorData);
    }
    
    // Serialization support (basic implementation)
    [[nodiscard]] std::string serialize() const override {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < value.size(); ++i) {
            if (i > 0) oss << ",";
            if (value[i]) {
                oss << value[i]->serialize();
            } else {
                oss << "null";
            }
        }
        oss << "]";
        return oss.str();
    }
    
    bool deserialize(std::string_view data) override {
        // Basic implementation - in a real scenario, you'd want proper JSON parsing
        // For now, just clear and return false to indicate unsupported
        (void)data;
        value.clear();
        return false;
    }
    
    // Container interface methods with security validation
    void push_back(std::shared_ptr<FrameworkData> item) {
        Security::BoundsChecker::check_not_null(item, "vector item");
        Security::InputValidator::validate_container_size(value, 1000000, "vector data");
        value.push_back(std::move(item));
    }
    
    void emplace_back(std::shared_ptr<FrameworkData> item) {
        Security::BoundsChecker::check_not_null(item, "vector item");
        Security::InputValidator::validate_container_size(value, 1000000, "vector data");
        value.emplace_back(std::move(item));
    }
    
    // Perfect forwarding emplace_back
    template<typename T, typename... Args>
    void emplace_back_typed(Args&&... args) {
        Security::InputValidator::validate_container_size(value, 1000000, "vector data");
        value.emplace_back(std::make_shared<T>(std::forward<Args>(args)...));
    }
    
    [[nodiscard]] bool empty() const noexcept {
        return value.empty();
    }
    
    [[nodiscard]] size_t size() const noexcept {
        return value.size();
    }
    
    void clear() noexcept {
        value.clear();
    }
    
    void reserve(size_t capacity) {
        Security::InputValidator::validate_container_size(std::vector<int>(capacity), 1000000, "vector reserve capacity");
        value.reserve(capacity);
    }
    
    // Safe element access with comprehensive bounds checking
    [[nodiscard]] std::shared_ptr<FrameworkData> at(size_t index) const {
        Security::BoundsChecker::check_bounds(value, index, "VectorData");
        return value[index];
    }
    
    // Iterator support
    auto begin() noexcept { return value.begin(); }
    auto end() noexcept { return value.end(); }
    [[nodiscard]] auto begin() const noexcept { return value.begin(); }
    [[nodiscard]] auto end() const noexcept { return value.end(); }
    [[nodiscard]] auto cbegin() const noexcept { return value.cbegin(); }
    [[nodiscard]] auto cend() const noexcept { return value.cend(); }
    
    // Reverse iterator support
    auto rbegin() noexcept { return value.rbegin(); }
    auto rend() noexcept { return value.rend(); }
    [[nodiscard]] auto rbegin() const noexcept { return value.rbegin(); }
    [[nodiscard]] auto rend() const noexcept { return value.rend(); }
    [[nodiscard]] auto crbegin() const noexcept { return value.crbegin(); }
    [[nodiscard]] auto crend() const noexcept { return value.crend(); }
    
    // Equality operators
    [[nodiscard]] bool operator==(const VectorData& other) const noexcept {
        if (value.size() != other.value.size()) {
            return false;
        }
        
        for (size_t i = 0; i < value.size(); ++i) {
            // Simple pointer comparison - in practice, you might want deep comparison
            if (value[i] != other.value[i]) {
                return false;
            }
        }
        return true;
    }
    
    [[nodiscard]] bool operator!=(const VectorData& other) const noexcept {
        return !(*this == other);
    }
};

#endif //VECTORDATA_H
