#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <utility>
#include <type_traits>

// Framework includes
#include "../BaseNode/BaseNode.h"
#include "../BaseNode/FrameworkData.h"
#include "../BaseNode/TypedData.h"
#include "../BaseNode/Result.h"
#include "../Flow/StringData.h"
#include "../BatchFlow/MapData.h"
#include "../BatchNode/VectorData.h"
#include "../AsyncNode/AsyncNode.h"

// Test fixture for move semantics and const-correctness tests
class MoveSemanticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data
        test_string_ = "Test String Data";
        large_string_ = std::string(10000, 'X');
    }
    
    void TearDown() override {
        // Cleanup if needed
    }
    
    std::string test_string_;
    std::string large_string_;
};

// Helper class to track move operations
class MoveTracker {
public:
    MoveTracker(int value) : value_(value), moved_from_(false) {}
    
    // Copy constructor
    MoveTracker(const MoveTracker& other) 
        : value_(other.value_), moved_from_(false) {
        copy_count_++;
    }
    
    // Move constructor
    MoveTracker(MoveTracker&& other) noexcept 
        : value_(other.value_), moved_from_(false) {
        other.moved_from_ = true;
        move_count_++;
    }
    
    // Copy assignment
    MoveTracker& operator=(const MoveTracker& other) {
        if (this != &other) {
            value_ = other.value_;
            moved_from_ = false;
            copy_count_++;
        }
        return *this;
    }
    
    // Move assignment
    MoveTracker& operator=(MoveTracker&& other) noexcept {
        if (this != &other) {
            value_ = other.value_;
            moved_from_ = false;
            other.moved_from_ = true;
            move_count_++;
        }
        return *this;
    }
    
    int value() const { return value_; }
    bool was_moved_from() const { return moved_from_; }
    
    static int copy_count_;
    static int move_count_;
    static void reset_counters() { copy_count_ = 0; move_count_ = 0; }
    
private:
    int value_;
    bool moved_from_;
};

int MoveTracker::copy_count_ = 0;
int MoveTracker::move_count_ = 0;

// Test StringData move semantics
TEST_F(MoveSemanticsTest, StringDataMoveSemantics) {
    // Test move constructor
    std::string original_string = large_string_;
    StringData string_data(std::move(original_string));
    
    EXPECT_EQ(string_data.value.size(), 10000);
    EXPECT_TRUE(original_string.empty()); // Should be moved from
    
    // Test move assignment
    std::string another_string(5000, 'Y');
    StringData another_data;
    another_data = StringData(std::move(another_string));
    
    EXPECT_EQ(another_data.value.size(), 5000);
    EXPECT_TRUE(another_string.empty()); // Should be moved from
    
    // Test that move constructor is noexcept
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<StringData>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<StringData>);
}

// Test VectorData move semantics
TEST_F(MoveSemanticsTest, VectorDataMoveSemantics) {
    // Create a large vector
    std::vector<std::shared_ptr<FrameworkData>> large_vector;
    for (int i = 0; i < 1000; ++i) {
        large_vector.push_back(std::make_shared<StringData>("Item " + std::to_string(i)));
    }
    
    // Test move constructor
    VectorData vector_data(std::move(large_vector));
    
    EXPECT_EQ(vector_data.value.size(), 1000);
    EXPECT_TRUE(large_vector.empty()); // Should be moved from
    
    // Test move assignment
    std::vector<std::shared_ptr<FrameworkData>> another_vector;
    for (int i = 0; i < 500; ++i) {
        another_vector.push_back(std::make_shared<StringData>("Another " + std::to_string(i)));
    }
    
    VectorData another_data;
    another_data = VectorData(std::move(another_vector));
    
    EXPECT_EQ(another_data.value.size(), 500);
    EXPECT_TRUE(another_vector.empty()); // Should be moved from
    
    // Test that move operations are noexcept
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<VectorData>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<VectorData>);
}

// Test MapData move semantics
TEST_F(MoveSemanticsTest, MapDataMoveSemantics) {
    // Create a large map
    std::map<std::string, std::shared_ptr<FrameworkData>> large_map;
    for (int i = 0; i < 1000; ++i) {
        large_map["key" + std::to_string(i)] = std::make_shared<StringData>("Value " + std::to_string(i));
    }
    
    // Test move constructor
    MapData map_data(std::move(large_map));
    
    EXPECT_EQ(map_data.value.size(), 1000);
    EXPECT_TRUE(large_map.empty()); // Should be moved from
    
    // Test move assignment
    std::map<std::string, std::shared_ptr<FrameworkData>> another_map;
    for (int i = 0; i < 250; ++i) {
        another_map["another" + std::to_string(i)] = std::make_shared<StringData>("AnotherValue " + std::to_string(i));
    }
    
    MapData another_data;
    another_data = MapData(std::move(another_map));
    
    EXPECT_EQ(another_data.value.size(), 250);
    EXPECT_TRUE(another_map.empty()); // Should be moved from
    
    // Test that move operations are noexcept
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<MapData>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<MapData>);
}

// Test TypedData move semantics
TEST_F(MoveSemanticsTest, TypedDataMoveSemantics) {
    MoveTracker::reset_counters();
    
    // Test move construction
    MoveTracker tracker(42);
    TypedData<MoveTracker> typed_data(std::move(tracker));
    
    EXPECT_EQ(typed_data.get().value(), 42);
    EXPECT_TRUE(tracker.was_moved_from());
    EXPECT_GT(MoveTracker::move_count_, 0); // At least one move should occur
    EXPECT_EQ(MoveTracker::copy_count_, 0);
    
    // Test move assignment
    MoveTracker::reset_counters();
    MoveTracker another_tracker(100);
    typed_data.set(std::move(another_tracker));
    
    EXPECT_EQ(typed_data.get().value(), 100);
    EXPECT_TRUE(another_tracker.was_moved_from());
    EXPECT_GT(MoveTracker::move_count_, 0); // At least one move should occur
    
    // Test that TypedData move operations are efficient
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<TypedData<MoveTracker>>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<TypedData<MoveTracker>>);
}

// Test Result<T> move semantics
TEST_F(MoveSemanticsTest, ResultMoveSemantics) {
    using namespace framework;
    
    MoveTracker::reset_counters();
    
    // Test move construction with value
    MoveTracker tracker(123);
    Result<MoveTracker> result(std::move(tracker));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().value(), 123);
    EXPECT_TRUE(tracker.was_moved_from());
    EXPECT_GT(MoveTracker::move_count_, 0); // At least one move should occur
    
    // Test move assignment
    MoveTracker::reset_counters();
    MoveTracker another_tracker(456);
    Result<MoveTracker> another_result(std::move(another_tracker));
    
    result = std::move(another_result);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().value(), 456);
    EXPECT_GT(MoveTracker::move_count_, 0);
    
    // Test move semantics with error
    Result<MoveTracker> error_result(FrameworkError::runtime_error("Test error"));
    Result<MoveTracker> moved_error = std::move(error_result);
    
    EXPECT_TRUE(moved_error.has_error());
    EXPECT_STREQ(moved_error.error().what(), "Test error");
    
    // Test that Result move operations are noexcept
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<Result<MoveTracker>>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<Result<MoveTracker>>);
}

// Test BaseNode parameter move semantics
TEST_F(MoveSemanticsTest, BaseNodeParameterMoveSemantics) {
    auto node = std::make_shared<BaseNode>();
    
    // Test move semantics in setParam
    std::map<std::string, std::shared_ptr<FrameworkData>> params;
    params["key1"] = std::make_shared<StringData>("value1");
    params["key2"] = std::make_shared<StringData>("value2");
    
    // Test move version of setParam
    node->setParam(std::move(params));
    
    EXPECT_TRUE(params.empty()); // Should be moved from
    EXPECT_EQ(node->getParams().size(), 2);
    
    // Verify parameters were moved correctly
    auto retrieved_param = node->getParam("key1");
    ASSERT_NE(retrieved_param, nullptr);
    auto string_data = retrieved_param->as<StringData>();
    ASSERT_NE(string_data, nullptr);
    EXPECT_EQ(string_data->value, "value1");
}

// Test const-correctness in BaseNode
TEST_F(MoveSemanticsTest, BaseNodeConstCorrectness) {
    auto node = std::make_shared<BaseNode>();
    
    // Set up some parameters
    std::map<std::string, std::shared_ptr<FrameworkData>> params;
    params["test_key"] = std::make_shared<StringData>("test_value");
    node->setParam(params);
    
    // Test const methods
    const BaseNode& const_node = *node;
    
    // These should compile and work with const reference
    auto const_param = const_node.getParam("test_key");
    auto const_params = const_node.getParams();
    auto const_successors = const_node.getSuccessor();
    
    EXPECT_NE(const_param, nullptr);
    EXPECT_EQ(const_params.size(), 1);
    EXPECT_TRUE(const_successors.empty());
    
    // Verify that const methods don't modify the object
    EXPECT_EQ(node->getParams().size(), 1);
}

// Test const-correctness in FrameworkData derived classes
TEST_F(MoveSemanticsTest, FrameworkDataConstCorrectness) {
    // Test StringData const methods
    const StringData string_data("test_string");
    
    EXPECT_EQ(string_data.get(), "test_string");
    EXPECT_EQ(string_data.size(), 11);
    EXPECT_FALSE(string_data.empty());
    EXPECT_EQ(string_data.serialize(), "test_string");
    
    // Test VectorData const methods
    VectorData vector_data;
    vector_data.push_back(std::make_shared<StringData>("item1"));
    vector_data.push_back(std::make_shared<StringData>("item2"));
    
    const VectorData& const_vector = vector_data;
    
    EXPECT_EQ(const_vector.size(), 2);
    EXPECT_FALSE(const_vector.empty());
    
    // Test const iterators
    int count = 0;
    for (const auto& item : const_vector) {
        EXPECT_NE(item, nullptr);
        count++;
    }
    EXPECT_EQ(count, 2);
    
    // Test MapData const methods
    MapData map_data;
    map_data.insert("key1", std::make_shared<StringData>("value1"));
    map_data.insert("key2", std::make_shared<StringData>("value2"));
    
    const MapData& const_map = map_data;
    
    EXPECT_EQ(const_map.size(), 2);
    EXPECT_FALSE(const_map.empty());
    EXPECT_TRUE(const_map.contains("key1"));
    
    // Test const iterators
    count = 0;
    for (const auto& [key, value] : const_map) {
        EXPECT_FALSE(key.empty());
        EXPECT_NE(value, nullptr);
        count++;
    }
    EXPECT_EQ(count, 2);
}

// Test perfect forwarding in template functions
TEST_F(MoveSemanticsTest, PerfectForwarding) {
    MoveTracker::reset_counters();
    
    // Test perfect forwarding in TypedData factory function
    MoveTracker tracker(789);
    auto typed_data = make_typed_data(std::move(tracker));
    
    EXPECT_EQ(typed_data->get().value(), 789);
    EXPECT_TRUE(tracker.was_moved_from());
    EXPECT_GT(MoveTracker::move_count_, 0);
    // Note: Some copies might occur during template instantiation, so we don't enforce zero copies
    
    // Test perfect forwarding with lvalue
    MoveTracker::reset_counters();
    MoveTracker lvalue_tracker(999);
    auto typed_data2 = make_typed_data(lvalue_tracker);
    
    EXPECT_EQ(typed_data2->get().value(), 999);
    EXPECT_FALSE(lvalue_tracker.was_moved_from()); // Should not be moved from
    // Note: The exact number of moves/copies depends on implementation details
    EXPECT_GT(MoveTracker::copy_count_, 0); // Should have copies
}

// Test move semantics with various argument types
TEST_F(MoveSemanticsTest, MoveWithVariousTypes) {
    // Test with primitive types
    TypedData<int> int_data(42);
    TypedData<int> moved_int = std::move(int_data);
    EXPECT_EQ(moved_int.get(), 42);
    
    // Test with string
    std::string test_str = "Hello World";
    TypedData<std::string> string_data(std::move(test_str));
    EXPECT_EQ(string_data.get(), "Hello World");
    EXPECT_TRUE(test_str.empty());
    
    // Test with vector
    std::vector<int> test_vec = {1, 2, 3, 4, 5};
    TypedData<std::vector<int>> vec_data(std::move(test_vec));
    EXPECT_EQ(vec_data.get().size(), 5);
    EXPECT_TRUE(test_vec.empty());
    
    // Test with custom type
    MoveTracker::reset_counters();
    MoveTracker custom_obj(12345);
    TypedData<MoveTracker> custom_data(std::move(custom_obj));
    
    EXPECT_EQ(custom_data.get().value(), 12345);
    EXPECT_TRUE(custom_obj.was_moved_from());
    EXPECT_GT(MoveTracker::move_count_, 0);
}

// Test compiler optimizations with const-correctness
TEST_F(MoveSemanticsTest, CompilerOptimizations) {
    // Test that const methods enable compiler optimizations
    const StringData const_string("optimization_test");
    
    // These calls should be optimizable by the compiler
    auto size1 = const_string.size();
    auto size2 = const_string.size();
    auto empty1 = const_string.empty();
    auto empty2 = const_string.empty();
    
    EXPECT_EQ(size1, size2);
    EXPECT_EQ(empty1, empty2);
    
    // Test with const TypedData
    const TypedData<std::string> const_typed("const_test");
    
    auto& ref1 = const_typed.get();
    auto& ref2 = const_typed.get();
    
    // Should return references to the same object
    EXPECT_EQ(&ref1, &ref2);
    EXPECT_EQ(ref1, "const_test");
}

// Performance test for move vs copy operations
TEST_F(MoveSemanticsTest, MoveVsCopyPerformance) {
    const size_t iterations = 1000;
    const size_t string_size = 10000;
    
    // Test copy performance
    auto start_copy = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        std::string large_str(string_size, 'A');
        StringData data(large_str); // Copy constructor
        (void)data; // Prevent optimization
    }
    auto end_copy = std::chrono::high_resolution_clock::now();
    
    // Test move performance
    auto start_move = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        std::string large_str(string_size, 'A');
        StringData data(std::move(large_str)); // Move constructor
        (void)data; // Prevent optimization
    }
    auto end_move = std::chrono::high_resolution_clock::now();
    
    auto copy_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_copy - start_copy);
    auto move_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_move - start_move);
    
    // Move should be significantly faster than copy for large strings
    EXPECT_LT(move_duration.count(), copy_duration.count());
    
    // Print performance results for manual verification
    std::cout << "Copy operations took: " << copy_duration.count() << " microseconds\n";
    std::cout << "Move operations took: " << move_duration.count() << " microseconds\n";
    std::cout << "Move is " << (static_cast<double>(copy_duration.count()) / move_duration.count()) 
              << "x faster than copy\n";
}

// Test exception safety with move operations
TEST_F(MoveSemanticsTest, MoveExceptionSafety) {
    // Test that move operations maintain strong exception safety
    
    // Create a TypedData with constraints that might throw
    std::vector<Constraint<std::string>> constraints;
    constraints.emplace_back(Constraints<std::string>::not_empty());
    
    std::string valid_string = "valid";
    TypedData<std::string> data(std::move(valid_string), std::move(constraints));
    
    EXPECT_TRUE(data.is_valid());
    EXPECT_EQ(data.get(), "valid");
    
    // Test move assignment with constraint validation
    std::string another_valid = "another_valid";
    TypedData<std::string> another_data(std::move(another_valid));
    
    // This should work without throwing
    EXPECT_NO_THROW(data = std::move(another_data));
    EXPECT_EQ(data.get(), "another_valid");
}

// Test that move semantics work correctly with inheritance
TEST_F(MoveSemanticsTest, MoveWithInheritance) {
    // Test that FrameworkData polymorphic behavior works with moves
    std::unique_ptr<FrameworkData> base_ptr = std::make_unique<StringData>("polymorphic_test");
    
    // Test clone (which should work with moved objects)
    auto cloned = base_ptr->clone();
    ASSERT_NE(cloned, nullptr);
    
    auto cloned_string = cloned->as<StringData>();
    ASSERT_NE(cloned_string, nullptr);
    EXPECT_EQ(cloned_string->value, "polymorphic_test");
    
    // Test move with polymorphic containers
    std::vector<std::unique_ptr<FrameworkData>> poly_vec;
    poly_vec.push_back(std::make_unique<StringData>("item1"));
    poly_vec.push_back(std::make_unique<StringData>("item2"));
    
    // Move the entire vector
    std::vector<std::unique_ptr<FrameworkData>> moved_vec = std::move(poly_vec);
    
    EXPECT_TRUE(poly_vec.empty());
    EXPECT_EQ(moved_vec.size(), 2);
    
    auto moved_string = moved_vec[0]->as<StringData>();
    ASSERT_NE(moved_string, nullptr);
    EXPECT_EQ(moved_string->value, "item1");
}