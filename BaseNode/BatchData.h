#ifndef BATCHDATA_H
#define BATCHDATA_H

#include "FrameworkData.h"
#include "TypedData.h"
#include "InputValidation.h"
#include <vector>
#include <memory>
#include <memory_resource>
#include <typeinfo>
#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <type_traits>

// Forward declaration
class BatchData;

// Memory pool manager for batch operations
class BatchMemoryPool {
private:
    std::unique_ptr<std::pmr::memory_resource> pool_;
    size_t pool_size_;
    
public:
    explicit BatchMemoryPool(size_t initial_size = 1024 * 1024) // 1MB default
        : pool_size_(initial_size) {
        // Use monotonic buffer resource for efficient batch allocations
        pool_ = std::make_unique<std::pmr::monotonic_buffer_resource>(pool_size_);
    }
    
    // Get the memory resource
    [[nodiscard]] std::pmr::memory_resource* get_resource() noexcept {
        return pool_.get();
    }
    
    // Reset the pool (releases all allocated memory)
    void reset() {
        pool_ = std::make_unique<std::pmr::monotonic_buffer_resource>(pool_size_);
    }
    
    // Get current pool size
    [[nodiscard]] size_t pool_size() const noexcept {
        return pool_size_;
    }
};

// Enhanced batch container with memory pooling and type safety
class BatchData : public FrameworkData {
private:
    std::vector<std::shared_ptr<FrameworkData>> items_;
    std::shared_ptr<BatchMemoryPool> memory_pool_;
    size_t reserved_capacity_;
    
public:
    // Type aliases for STL compatibility
    using value_type = std::shared_ptr<FrameworkData>;
    using size_type = std::vector<value_type>::size_type;
    using difference_type = std::vector<value_type>::difference_type;
    using reference = std::vector<value_type>::reference;
    using const_reference = std::vector<value_type>::const_reference;
    using iterator = std::vector<value_type>::iterator;
    using const_iterator = std::vector<value_type>::const_iterator;
    using reverse_iterator = std::vector<value_type>::reverse_iterator;
    using const_reverse_iterator = std::vector<value_type>::const_reverse_iterator;
    
    // Constructors
    BatchData() : reserved_capacity_(0) {
        memory_pool_ = std::make_shared<BatchMemoryPool>();
    }
    
    explicit BatchData(size_t reserve_size) : reserved_capacity_(reserve_size) {
        memory_pool_ = std::make_shared<BatchMemoryPool>();
        if (reserve_size > 0) {
            items_.reserve(reserve_size);
        }
    }
    
    explicit BatchData(std::shared_ptr<BatchMemoryPool> pool, size_t reserve_size = 0) 
        : memory_pool_(std::move(pool)), reserved_capacity_(reserve_size) {
        if (!memory_pool_) {
            memory_pool_ = std::make_shared<BatchMemoryPool>();
        }
        if (reserve_size > 0) {
            items_.reserve(reserve_size);
        }
    }
    
    // Copy constructor with deep copy
    BatchData(const BatchData& other) 
        : reserved_capacity_(other.reserved_capacity_) {
        memory_pool_ = std::make_shared<BatchMemoryPool>();
        items_.reserve(other.items_.size());
        for (const auto& item : other.items_) {
            if (item) {
                items_.push_back(item->clone());
            } else {
                items_.push_back(nullptr);
            }
        }
    }
    
    // Move constructor
    BatchData(BatchData&& other) noexcept 
        : items_(std::move(other.items_))
        , memory_pool_(std::move(other.memory_pool_))
        , reserved_capacity_(other.reserved_capacity_) {
        other.reserved_capacity_ = 0;
    }
    
    // Copy assignment with deep copy
    BatchData& operator=(const BatchData& other) {
        if (this != &other) {
            items_.clear();
            memory_pool_ = std::make_shared<BatchMemoryPool>();
            reserved_capacity_ = other.reserved_capacity_;
            items_.reserve(other.items_.size());
            for (const auto& item : other.items_) {
                if (item) {
                    items_.push_back(item->clone());
                } else {
                    items_.push_back(nullptr);
                }
            }
        }
        return *this;
    }
    
    // Move assignment
    BatchData& operator=(BatchData&& other) noexcept {
        if (this != &other) {
            items_ = std::move(other.items_);
            memory_pool_ = std::move(other.memory_pool_);
            reserved_capacity_ = other.reserved_capacity_;
            other.reserved_capacity_ = 0;
        }
        return *this;
    }
    
    // FrameworkData interface implementation
    [[nodiscard]] std::unique_ptr<FrameworkData> clone() const override {
        return std::make_unique<BatchData>(*this);
    }
    
    [[nodiscard]] const std::type_info& type() const noexcept override {
        return typeid(BatchData);
    }
    
    // Serialization support
    std::string serialize() const override {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < items_.size(); ++i) {
            if (i > 0) oss << ",";
            if (items_[i]) {
                oss << items_[i]->serialize();
            } else {
                oss << "null";
            }
        }
        oss << "]";
        return oss.str();
    }
    
    bool deserialize(std::string_view data) override {
        // Basic implementation - in production, you'd want proper JSON parsing
        (void)data;
        items_.clear();
        return false; // Indicate unsupported for now
    }
    
    // Container interface methods with efficient memory usage
    void reserve(size_t capacity) {
        items_.reserve(capacity);
        reserved_capacity_ = capacity;
    }
    
    void resize(size_t count) {
        items_.resize(count);
    }
    
    void resize(size_t count, const std::shared_ptr<FrameworkData>& value) {
        items_.resize(count, value);
    }
    
    // Type-safe item addition methods with security validation
    void add_item(std::shared_ptr<FrameworkData> item) {
        Security::BoundsChecker::check_not_null(item, "batch item");
        
        // Prevent excessive memory usage
        Security::InputValidator::validate_container_size(items_, 1000000, "batch items");
        
        items_.emplace_back(std::move(item));
    }
    
    template<typename T>
    void add_typed_item(T&& item) {
        static_assert(!std::is_pointer_v<std::decay_t<T>>, 
                     "Use add_item() for pointer types");
        items_.emplace_back(
            std::make_shared<TypedData<std::decay_t<T>>>(std::forward<T>(item))
        );
    }
    
    template<typename T>
    void add_typed_item_with_constraints(T&& item, std::vector<Constraint<std::decay_t<T>>> constraints) {
        static_assert(!std::is_pointer_v<std::decay_t<T>>, 
                     "Use add_item() for pointer types");
        items_.emplace_back(
            std::make_shared<TypedData<std::decay_t<T>>>(
                std::forward<T>(item), std::move(constraints)
            )
        );
    }
    
    // Batch addition methods for efficiency
    template<typename InputIterator>
    void add_range(InputIterator first, InputIterator last) {
        static_assert(std::is_convertible_v<
            typename std::iterator_traits<InputIterator>::value_type,
            std::shared_ptr<FrameworkData>
        >, "Iterator must point to FrameworkData-compatible types");
        
        auto distance = std::distance(first, last);
        if (distance > 0) {
            items_.reserve(items_.size() + distance);
            for (auto it = first; it != last; ++it) {
                if (*it) {
                    items_.emplace_back(*it);
                } else {
                    throw std::invalid_argument("Cannot add null item to batch");
                }
            }
        }
    }
    
    template<typename Container>
    void add_container(const Container& container) {
        items_.reserve(items_.size() + container.size());
        for (const auto& item : container) {
            add_item(item);
        }
    }
    
    // Efficient bulk operations
    void push_back(std::shared_ptr<FrameworkData> item) {
        add_item(std::move(item));
    }
    
    template<typename... Args>
    void emplace_back(Args&&... args) {
        items_.emplace_back(std::forward<Args>(args)...);
    }
    
    void pop_back() {
        Security::InputValidator::validate_non_empty_container(items_, "batch");
        items_.pop_back();
    }
    
    // Element access with comprehensive bounds checking
    [[nodiscard]] std::shared_ptr<FrameworkData> at(size_t index) const {
        Security::BoundsChecker::check_bounds(items_, index, "BatchData");
        return items_[index];
    }
    
    [[nodiscard]] std::shared_ptr<FrameworkData> operator[](size_t index) const noexcept {
        return items_[index];
    }
    
    [[nodiscard]] std::shared_ptr<FrameworkData>& operator[](size_t index) noexcept {
        return items_[index];
    }
    
    [[nodiscard]] std::shared_ptr<FrameworkData> front() const {
        Security::InputValidator::validate_non_empty_container(items_, "batch");
        return items_.front();
    }
    
    [[nodiscard]] std::shared_ptr<FrameworkData> back() const {
        Security::InputValidator::validate_non_empty_container(items_, "batch");
        return items_.back();
    }
    
    // Type-safe element access with security validation
    template<typename T>
    [[nodiscard]] const T* get_typed_item(size_t index) const {
        auto item = at(index);
        return Security::SafeCaster::optional_cast<T>(item.get());
    }
    
    template<typename T>
    [[nodiscard]] const T& get_typed_item_safe(size_t index) const {
        auto item = at(index);
        Security::BoundsChecker::check_not_null(item, "batch item at index " + std::to_string(index));
        return *Security::SafeCaster::safe_cast<T>(item.get(), "batch item type cast");
    }
    
    // Container properties
    [[nodiscard]] bool empty() const noexcept {
        return items_.empty();
    }
    
    [[nodiscard]] size_t size() const noexcept {
        return items_.size();
    }
    
    [[nodiscard]] size_t capacity() const noexcept {
        return items_.capacity();
    }
    
    [[nodiscard]] size_t max_size() const noexcept {
        return items_.max_size();
    }
    
    void clear() noexcept {
        items_.clear();
        if (memory_pool_) {
            memory_pool_->reset();
        }
    }
    
    void shrink_to_fit() {
        items_.shrink_to_fit();
    }
    
    // Iterator support for range-based operations
    [[nodiscard]] iterator begin() noexcept { return items_.begin(); }
    [[nodiscard]] iterator end() noexcept { return items_.end(); }
    [[nodiscard]] const_iterator begin() const noexcept { return items_.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return items_.end(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return items_.cbegin(); }
    [[nodiscard]] const_iterator cend() const noexcept { return items_.cend(); }
    
    [[nodiscard]] reverse_iterator rbegin() noexcept { return items_.rbegin(); }
    [[nodiscard]] reverse_iterator rend() noexcept { return items_.rend(); }
    [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return items_.rbegin(); }
    [[nodiscard]] const_reverse_iterator rend() const noexcept { return items_.rend(); }
    [[nodiscard]] const_reverse_iterator crbegin() const noexcept { return items_.crbegin(); }
    [[nodiscard]] const_reverse_iterator crend() const noexcept { return items_.crend(); }
    
    // Memory management
    [[nodiscard]] std::shared_ptr<BatchMemoryPool> get_memory_pool() const noexcept {
        return memory_pool_;
    }
    
    void reset_memory_pool() {
        if (memory_pool_) {
            memory_pool_->reset();
        }
    }
    
    // Batch processing utilities
    template<typename Predicate>
    [[nodiscard]] size_t count_if(Predicate pred) const {
        return std::count_if(items_.begin(), items_.end(), pred);
    }
    
    template<typename Predicate>
    [[nodiscard]] iterator find_if(Predicate pred) {
        return std::find_if(items_.begin(), items_.end(), pred);
    }
    
    template<typename Predicate>
    [[nodiscard]] const_iterator find_if(Predicate pred) const {
        return std::find_if(items_.begin(), items_.end(), pred);
    }
    
    template<typename UnaryFunction>
    void for_each(UnaryFunction func) {
        std::for_each(items_.begin(), items_.end(), func);
    }
    
    template<typename UnaryFunction>
    void for_each(UnaryFunction func) const {
        std::for_each(items_.begin(), items_.end(), func);
    }
    
    // Type filtering utilities
    template<typename T>
    [[nodiscard]] std::vector<std::shared_ptr<T>> filter_by_type() const {
        std::vector<std::shared_ptr<T>> result;
        for (const auto& item : items_) {
            if (item && item->template is<T>()) {
                result.push_back(std::static_pointer_cast<T>(item));
            }
        }
        return result;
    }
    
    template<typename T>
    [[nodiscard]] size_t count_by_type() const {
        return count_if([](const auto& item) {
            return item && item->template is<T>();
        });
    }
    
    // Batch validation
    [[nodiscard]] bool all_valid() const noexcept {
        return std::all_of(items_.begin(), items_.end(), 
                          [](const auto& item) { return item != nullptr; });
    }
    
    [[nodiscard]] std::vector<size_t> get_invalid_indices() const {
        std::vector<size_t> invalid_indices;
        for (size_t i = 0; i < items_.size(); ++i) {
            if (!items_[i]) {
                invalid_indices.push_back(i);
            }
        }
        return invalid_indices;
    }
    
    // Remove invalid items
    void remove_invalid_items() {
        items_.erase(
            std::remove_if(items_.begin(), items_.end(),
                          [](const auto& item) { return !item; }),
            items_.end()
        );
    }
    
    // Equality operators
    bool operator==(const BatchData& other) const noexcept {
        if (items_.size() != other.items_.size()) {
            return false;
        }
        
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i] != other.items_[i]) {
                return false;
            }
        }
        return true;
    }
    
    bool operator!=(const BatchData& other) const noexcept {
        return !(*this == other);
    }
};

// Factory functions for easier BatchData creation
[[nodiscard]] inline std::unique_ptr<BatchData> make_batch_data(size_t reserve_size = 0) {
    return std::make_unique<BatchData>(reserve_size);
}

[[nodiscard]] inline std::unique_ptr<BatchData> make_batch_data_with_pool(
    std::shared_ptr<BatchMemoryPool> pool, size_t reserve_size = 0) {
    return std::make_unique<BatchData>(std::move(pool), reserve_size);
}

// Utility function to create a batch from a container
template<typename Container>
[[nodiscard]] std::unique_ptr<BatchData> make_batch_from_container(const Container& container) {
    auto batch = std::make_unique<BatchData>(container.size());
    batch->add_container(container);
    return batch;
}

// Utility function to create a batch from an iterator range
template<typename InputIterator>
[[nodiscard]] std::unique_ptr<BatchData> make_batch_from_range(InputIterator first, InputIterator last) {
    auto distance = std::distance(first, last);
    auto batch = std::make_unique<BatchData>(distance > 0 ? distance : 0);
    if (distance > 0) {
        batch->add_range(first, last);
    }
    return batch;
}

#endif //BATCHDATA_H