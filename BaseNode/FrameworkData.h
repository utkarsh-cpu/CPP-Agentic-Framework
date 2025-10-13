// FrameworkData.h
#ifndef FRAMEWORKDATA_H
#define FRAMEWORKDATA_H

#include <memory>
#include <typeinfo>
#include <string>
#include <stdexcept>

struct FrameworkData {
    virtual ~FrameworkData() = default;
    
    // Pure virtual methods that must be implemented by derived classes
    [[nodiscard]] virtual std::unique_ptr<FrameworkData> clone() const = 0;
    [[nodiscard]] virtual const std::type_info& type() const noexcept = 0;
    
    // Optional serialization support interface
    [[nodiscard]] virtual std::string serialize() const { return ""; }
    virtual bool deserialize(std::string_view data) { (void)data; return false; }
    
    // Type-safe casting helpers with proper error handling
    template<typename T>
    [[nodiscard]] const T* as() const noexcept {
        return dynamic_cast<const T*>(this);
    }
    
    template<typename T>
    [[nodiscard]] T* as() noexcept {
        return dynamic_cast<T*>(this);
    }
    
    // Safe casting with exception on failure
    template<typename T>
    [[nodiscard]] const T& safe_as() const {
        const T* result = dynamic_cast<const T*>(this);
        if (!result) {
            throw std::bad_cast();
        }
        return *result;
    }
    
    template<typename T>
    [[nodiscard]] T& safe_as() {
        T* result = dynamic_cast<T*>(this);
        if (!result) {
            throw std::bad_cast();
        }
        return *result;
    }
    
    // Type checking helper
    template<typename T>
    [[nodiscard]] bool is() const noexcept {
        return dynamic_cast<const T*>(this) != nullptr;
    }
};

#endif //FRAMEWORKDATA_H
