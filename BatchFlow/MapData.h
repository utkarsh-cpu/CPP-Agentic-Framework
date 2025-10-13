#ifndef MAPDATA_H
#define MAPDATA_H

#include "../BaseNode/FrameworkData.h"
#include "../BaseNode/InputValidation.h"
#include <map>
#include <string>
#include <memory>
#include <typeinfo>
#include <sstream>
#include <stdexcept>

struct MapData : public FrameworkData {
    std::map<std::string, std::shared_ptr<FrameworkData>> value;
    
    // Constructors
    MapData() = default;
    explicit MapData(std::map<std::string, std::shared_ptr<FrameworkData>> val) 
        : value(std::move(val)) {}
    
    // Perfect forwarding constructor for initializer list
    template<typename... Args>
    explicit MapData(Args&&... args) : value{std::forward<Args>(args)...} {}
    
    // Copy constructor with deep copy
    MapData(const MapData& other) {
        for (const auto& [key, item] : other.value) {
            if (item) {
                value[key] = item->clone();
            } else {
                value[key] = nullptr;
            }
        }
    }
    
    // Copy assignment with deep copy
    MapData& operator=(const MapData& other) {
        if (this != &other) {
            value.clear();
            for (const auto& [key, item] : other.value) {
                if (item) {
                    value[key] = item->clone();
                } else {
                    value[key] = nullptr;
                }
            }
        }
        return *this;
    }
    
    // Move constructor and assignment
    MapData(MapData&& other) noexcept : value(std::move(other.value)) {}
    MapData& operator=(MapData&& other) noexcept {
        if (this != &other) {
            value = std::move(other.value);
        }
        return *this;
    }
    
    // FrameworkData interface implementation
    [[nodiscard]] std::unique_ptr<FrameworkData> clone() const override {
        return std::make_unique<MapData>(*this);
    }
    
    [[nodiscard]] const std::type_info& type() const noexcept override {
        return typeid(MapData);
    }
    
    // Serialization support (basic implementation)
    [[nodiscard]] std::string serialize() const override {
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        for (const auto& [key, item] : value) {
            if (!first) oss << ",";
            first = false;
            oss << "\"" << key << "\":";
            if (item) {
                oss << item->serialize();
            } else {
                oss << "null";
            }
        }
        oss << "}";
        return oss.str();
    }
    
    bool deserialize(std::string_view data) override {
        // Basic implementation - in a real scenario, you'd want proper JSON parsing
        // For now, just clear and return false to indicate unsupported
        (void)data;
        value.clear();
        return false;
    }
    
    // Map interface methods with security validation
    void insert(std::string_view key, std::shared_ptr<FrameworkData> item) {
        std::string key_str{key};
        Security::InputValidator::validate_non_empty_string(key_str, "map key");
        Security::InputValidator::validate_string_length(key_str, 256, "map key");
        Security::BoundsChecker::check_not_null(item, "map item");
        Security::InputValidator::validate_container_size(value, 10000, "map data");
        value[key_str] = std::move(item);
    }
    
    void emplace(std::string_view key, std::shared_ptr<FrameworkData> item) {
        std::string key_str{key};
        Security::InputValidator::validate_non_empty_string(key_str, "map key");
        Security::InputValidator::validate_string_length(key_str, 256, "map key");
        Security::BoundsChecker::check_not_null(item, "map item");
        Security::InputValidator::validate_container_size(value, 10000, "map data");
        value.emplace(key_str, std::move(item));
    }
    
    // Perfect forwarding emplace
    template<typename T, typename... Args>
    void emplace_typed(std::string_view key, Args&&... args) {
        std::string key_str{key};
        Security::InputValidator::validate_non_empty_string(key_str, "map key");
        Security::InputValidator::validate_string_length(key_str, 256, "map key");
        Security::InputValidator::validate_container_size(value, 10000, "map data");
        value.emplace(key_str, std::make_shared<T>(std::forward<Args>(args)...));
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
    
    [[nodiscard]] bool contains(std::string_view key) const {
        return value.find(std::string{key}) != value.end();
    }
    
    // Safe element access with input validation
    [[nodiscard]] std::shared_ptr<FrameworkData> at(std::string_view key) const {
        std::string key_str{key};
        Security::InputValidator::validate_non_empty_string(key_str, "map key");
        Security::InputValidator::validate_string_length(key_str, 256, "map key");
        
        auto it = value.find(key_str);
        if (it == value.end()) {
            throw Security::InputValidationException("MapData key not found: " + key_str);
        }
        return it->second;
    }
    
    // Element access with default
    [[nodiscard]] std::shared_ptr<FrameworkData> get(std::string_view key, 
                                                     std::shared_ptr<FrameworkData> default_value = nullptr) const {
        auto it = value.find(std::string{key});
        return (it != value.end()) ? it->second : std::move(default_value);
    }
    
    // Remove element
    bool erase(std::string_view key) {
        return value.erase(std::string{key}) > 0;
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
    
    // Find method
    auto find(std::string_view key) { return value.find(std::string{key}); }
    [[nodiscard]] auto find(std::string_view key) const { return value.find(std::string{key}); }
    
    // Equality operators
    [[nodiscard]] bool operator==(const MapData& other) const noexcept {
        if (value.size() != other.value.size()) {
            return false;
        }
        
        for (const auto& [key, item] : value) {
            auto it = other.value.find(key);
            if (it == other.value.end() || it->second != item) {
                return false;
            }
        }
        return true;
    }
    
    [[nodiscard]] bool operator!=(const MapData& other) const noexcept {
        return !(*this == other);
    }
};

#endif //MAPDATA_H
