#ifndef STRINGDATA_H
#define STRINGDATA_H

#include "../BaseNode/FrameworkData.h"
#include <string>
#include <memory>
#include <typeinfo>

struct StringData : public FrameworkData {
    std::string value;
    
    // Constructors
    StringData() = default;
    explicit StringData(std::string val) : value(std::move(val)) {}
    explicit StringData(const char* val) : value(val) {}
    
    // Copy constructor and assignment
    StringData(const StringData& other) : value(other.value) {}
    StringData& operator=(const StringData& other) {
        if (this != &other) {
            value = other.value;
        }
        return *this;
    }
    
    // Move constructor and assignment
    StringData(StringData&& other) noexcept : value(std::move(other.value)) {}
    StringData& operator=(StringData&& other) noexcept {
        if (this != &other) {
            value = std::move(other.value);
        }
        return *this;
    }
    
    // FrameworkData interface implementation
    [[nodiscard]] std::unique_ptr<FrameworkData> clone() const override {
        return std::make_unique<StringData>(value);
    }
    
    [[nodiscard]] const std::type_info& type() const noexcept override {
        return typeid(StringData);
    }
    
    // Serialization support
    [[nodiscard]] std::string serialize() const override {
        return value;
    }
    
    bool deserialize(std::string_view data) override {
        value = data;
        return true;
    }
    
    // Convenience methods
    [[nodiscard]] bool empty() const noexcept {
        return value.empty();
    }
    
    [[nodiscard]] size_t size() const noexcept {
        return value.size();
    }
    
    [[nodiscard]] const std::string& get() const noexcept {
        return value;
    }
    
    [[nodiscard]] std::string& get() noexcept {
        return value;
    }
    
    // Equality operators
    [[nodiscard]] bool operator==(const StringData& other) const noexcept {
        return value == other.value;
    }
    
    [[nodiscard]] bool operator!=(const StringData& other) const noexcept {
        return !(*this == other);
    }
    
    // String comparison operators
    [[nodiscard]] bool operator==(std::string_view other) const noexcept {
        return value == other;
    }
    
    [[nodiscard]] bool operator!=(std::string_view other) const noexcept {
        return value != other;
    }
};

#endif //STRINGDATA_H
