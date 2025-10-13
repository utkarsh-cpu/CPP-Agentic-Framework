#include <gtest/gtest.h>
#include "../BaseNode/BatchData.h"
#include "../BaseNode/TypedData.h"
#include "../Flow/StringData.h"
#include <memory>
#include <string>
#include <vector>

class BatchDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create some test data
        string_data1 = std::make_shared<StringData>("test1");
        string_data2 = std::make_shared<StringData>("test2");
        int_data1 = std::make_shared<TypedData<int>>(42);
        int_data2 = std::make_shared<TypedData<int>>(100);
    }
    
    std::shared_ptr<StringData> string_data1;
    std::shared_ptr<StringData> string_data2;
    std::shared_ptr<TypedData<int>> int_data1;
    std::shared_ptr<TypedData<int>> int_data2;
};

// Test basic construction and memory pooling
TEST_F(BatchDataTest, ConstructionAndMemoryPooling) {
    // Default constructor
    BatchData batch1;
    EXPECT_TRUE(batch1.empty());
    EXPECT_EQ(batch1.size(), 0);
    EXPECT_NE(batch1.get_memory_pool(), nullptr);
    
    // Constructor with reserve size
    BatchData batch2(100);
    EXPECT_TRUE(batch2.empty());
    EXPECT_EQ(batch2.size(), 0);
    EXPECT_GE(batch2.capacity(), 100);
    
    // Constructor with custom memory pool
    auto pool = std::make_shared<BatchMemoryPool>(2048);
    BatchData batch3(pool, 50);
    EXPECT_TRUE(batch3.empty());
    EXPECT_EQ(batch3.size(), 0);
    EXPECT_EQ(batch3.get_memory_pool(), pool);
    EXPECT_GE(batch3.capacity(), 50);
}

// Test efficient batch container with proper reserve() usage
TEST_F(BatchDataTest, EfficientBatchContainer) {
    BatchData batch;
    
    // Test reserve functionality
    batch.reserve(1000);
    EXPECT_GE(batch.capacity(), 1000);
    EXPECT_EQ(batch.size(), 0);
    
    // Add items and verify capacity is maintained
    for (int i = 0; i < 500; ++i) {
        batch.add_typed_item(i);
    }
    EXPECT_EQ(batch.size(), 500);
    EXPECT_GE(batch.capacity(), 1000); // Should not have reallocated
    
    // Test resize
    batch.resize(750);
    EXPECT_EQ(batch.size(), 750);
    
    // Test shrink_to_fit
    batch.shrink_to_fit();
    EXPECT_EQ(batch.capacity(), batch.size());
}

// Test range-based iteration support
TEST_F(BatchDataTest, RangeBasedIteration) {
    BatchData batch;
    batch.add_item(string_data1);
    batch.add_item(int_data1);
    batch.add_item(string_data2);
    batch.add_item(int_data2);
    
    // Test range-based for loop
    size_t count = 0;
    for (const auto& item : batch) {
        EXPECT_NE(item, nullptr);
        ++count;
    }
    EXPECT_EQ(count, 4);
    
    // Test STL algorithms
    auto non_null_count = batch.count_if([](const auto& item) {
        return item != nullptr;
    });
    EXPECT_EQ(non_null_count, 4);
    
    // Test iterator types
    auto it = batch.begin();
    auto end_it = batch.end();
    EXPECT_NE(it, end_it);
    
    // Test const iterators
    const BatchData& const_batch = batch;
    auto const_it = const_batch.cbegin();
    auto const_end = const_batch.cend();
    EXPECT_NE(const_it, const_end);
    
    // Test reverse iterators
    auto rit = batch.rbegin();
    auto rend_it = batch.rend();
    EXPECT_NE(rit, rend_it);
}

// Test type-safe batch item addition methods
TEST_F(BatchDataTest, TypeSafeBatchItemAddition) {
    BatchData batch;
    
    // Test add_item with shared_ptr
    batch.add_item(string_data1);
    batch.add_item(int_data1);
    EXPECT_EQ(batch.size(), 2);
    
    // Test add_typed_item with direct values
    batch.add_typed_item(std::string("direct_string"));
    batch.add_typed_item(42);
    batch.add_typed_item(3.14);
    EXPECT_EQ(batch.size(), 5);
    
    // Test type-safe access
    auto typed_string = batch.get_typed_item<TypedData<std::string>>(2);
    EXPECT_NE(typed_string, nullptr);
    EXPECT_EQ(typed_string->get(), "direct_string");
    
    auto typed_int = batch.get_typed_item<TypedData<int>>(3);
    EXPECT_NE(typed_int, nullptr);
    EXPECT_EQ(typed_int->get(), 42);
    
    auto typed_double = batch.get_typed_item<TypedData<double>>(4);
    EXPECT_NE(typed_double, nullptr);
    EXPECT_DOUBLE_EQ(typed_double->get(), 3.14);
    
    // Test safe access
    const auto& safe_string = batch.get_typed_item_safe<TypedData<std::string>>(2);
    EXPECT_EQ(safe_string.get(), "direct_string");
}

// Test batch addition methods for efficiency
TEST_F(BatchDataTest, BatchAdditionMethods) {
    BatchData batch;
    
    // Test add_range
    std::vector<std::shared_ptr<FrameworkData>> items = {
        string_data1, int_data1, string_data2, int_data2
    };
    batch.add_range(items.begin(), items.end());
    EXPECT_EQ(batch.size(), 4);
    
    // Test add_container
    BatchData batch2;
    std::vector<std::shared_ptr<FrameworkData>> more_items = {
        std::make_shared<TypedData<int>>(999),
        std::make_shared<StringData>("container_test")
    };
    batch2.add_container(more_items);
    EXPECT_EQ(batch2.size(), 2);
    
    // Test emplace_back
    batch.emplace_back(std::make_shared<TypedData<bool>>(true));
    EXPECT_EQ(batch.size(), 5);
}

// Test element access with bounds checking
TEST_F(BatchDataTest, ElementAccess) {
    BatchData batch;
    batch.add_item(string_data1);
    batch.add_item(int_data1);
    batch.add_item(string_data2);
    
    // Test at() with valid indices
    EXPECT_EQ(batch.at(0), string_data1);
    EXPECT_EQ(batch.at(1), int_data1);
    EXPECT_EQ(batch.at(2), string_data2);
    
    // Test at() with invalid index
    EXPECT_THROW(batch.at(3), std::out_of_range);
    
    // Test operator[]
    EXPECT_EQ(batch[0], string_data1);
    EXPECT_EQ(batch[1], int_data1);
    
    // Test front() and back()
    EXPECT_EQ(batch.front(), string_data1);
    EXPECT_EQ(batch.back(), string_data2);
    
    // Test front() and back() on empty batch
    BatchData empty_batch;
    EXPECT_THROW(empty_batch.front(), std::runtime_error);
    EXPECT_THROW(empty_batch.back(), std::runtime_error);
}

// Test type filtering utilities
TEST_F(BatchDataTest, TypeFilteringUtilities) {
    BatchData batch;
    batch.add_item(string_data1);
    batch.add_item(int_data1);
    batch.add_item(string_data2);
    batch.add_item(int_data2);
    
    // Test filter_by_type
    auto string_items = batch.filter_by_type<StringData>();
    EXPECT_EQ(string_items.size(), 2);
    
    auto int_items = batch.filter_by_type<TypedData<int>>();
    EXPECT_EQ(int_items.size(), 2);
    
    // Test count_by_type
    EXPECT_EQ(batch.count_by_type<StringData>(), 2);
    EXPECT_EQ(batch.count_by_type<TypedData<int>>(), 2);
    EXPECT_EQ(batch.count_by_type<TypedData<double>>(), 0);
}

// Test batch validation
TEST_F(BatchDataTest, BatchValidation) {
    BatchData batch;
    batch.add_item(string_data1);
    batch.add_item(int_data1);
    
    // All items are valid
    EXPECT_TRUE(batch.all_valid());
    EXPECT_TRUE(batch.get_invalid_indices().empty());
    
    // Add a null item
    batch.emplace_back(nullptr);
    EXPECT_FALSE(batch.all_valid());
    
    auto invalid_indices = batch.get_invalid_indices();
    EXPECT_EQ(invalid_indices.size(), 1);
    EXPECT_EQ(invalid_indices[0], 2);
    
    // Remove invalid items
    batch.remove_invalid_items();
    EXPECT_EQ(batch.size(), 2);
    EXPECT_TRUE(batch.all_valid());
}

// Test copy and move semantics
TEST_F(BatchDataTest, CopyAndMoveSemantics) {
    BatchData original;
    original.add_item(string_data1);
    original.add_item(int_data1);
    
    // Test copy constructor
    BatchData copied(original);
    EXPECT_EQ(copied.size(), original.size());
    EXPECT_NE(copied.get_memory_pool(), original.get_memory_pool()); // Different pools
    
    // Test move constructor
    BatchData moved(std::move(original));
    EXPECT_EQ(moved.size(), 2);
    EXPECT_EQ(original.size(), 0); // Original should be empty after move
    
    // Test copy assignment
    BatchData copy_assigned;
    copy_assigned = copied;
    EXPECT_EQ(copy_assigned.size(), copied.size());
    
    // Test move assignment
    BatchData move_assigned;
    move_assigned = std::move(copied);
    EXPECT_EQ(move_assigned.size(), 2);
    EXPECT_EQ(copied.size(), 0); // Original should be empty after move
}

// Test serialization
TEST_F(BatchDataTest, Serialization) {
    BatchData batch;
    batch.add_item(string_data1);
    batch.add_typed_item(42);
    
    // Test serialization
    std::string serialized = batch.serialize();
    EXPECT_FALSE(serialized.empty());
    EXPECT_NE(serialized.find("["), std::string::npos);
    EXPECT_NE(serialized.find("]"), std::string::npos);
    
    // Test clone
    auto cloned = batch.clone();
    EXPECT_NE(cloned, nullptr);
    
    auto cloned_batch = dynamic_cast<BatchData*>(cloned.get());
    EXPECT_NE(cloned_batch, nullptr);
    EXPECT_EQ(cloned_batch->size(), batch.size());
}

// Test factory functions
TEST_F(BatchDataTest, FactoryFunctions) {
    // Test make_batch_data
    auto batch1 = make_batch_data(100);
    EXPECT_NE(batch1, nullptr);
    EXPECT_GE(batch1->capacity(), 100);
    
    // Test make_batch_data_with_pool
    auto pool = std::make_shared<BatchMemoryPool>();
    auto batch2 = make_batch_data_with_pool(pool, 50);
    EXPECT_NE(batch2, nullptr);
    EXPECT_EQ(batch2->get_memory_pool(), pool);
    
    // Test make_batch_from_container
    std::vector<std::shared_ptr<FrameworkData>> items = {string_data1, int_data1};
    auto batch3 = make_batch_from_container(items);
    EXPECT_NE(batch3, nullptr);
    EXPECT_EQ(batch3->size(), 2);
    
    // Test make_batch_from_range
    auto batch4 = make_batch_from_range(items.begin(), items.end());
    EXPECT_NE(batch4, nullptr);
    EXPECT_EQ(batch4->size(), 2);
}

// Test error handling
TEST_F(BatchDataTest, ErrorHandling) {
    BatchData batch;
    
    // Test adding null item
    EXPECT_THROW(batch.add_item(nullptr), std::invalid_argument);
    
    // Test pop_back on empty batch
    EXPECT_THROW(batch.pop_back(), std::runtime_error);
    
    // Test safe access with wrong type
    batch.add_item(string_data1);
    EXPECT_THROW(batch.get_typed_item_safe<TypedData<int>>(0), std::bad_cast);
}