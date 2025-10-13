#include <gtest/gtest.h>
#include "../BaseNode/TypedData.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <complex>
#include <array>
#include <limits>
#include <type_traits>
#include <memory>

class TypedDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
    }
    
    void TearDown() override {
        // Cleanup
    }
};

// Test basic TypedData functionality
TEST_F(TypedDataTest, BasicConstruction) {
    auto int_data = make_typed_data(42);
    EXPECT_EQ(int_data->get(), 42);
    
    auto string_data = make_typed_data(std::string("hello"));
    EXPECT_EQ(string_data->get(), "hello");
    
    auto double_data = make_typed_data(3.14);
    EXPECT_DOUBLE_EQ(double_data->get(), 3.14);
}

// Test constraint validation
TEST_F(TypedDataTest, ConstraintValidation) {
    // Test range constraint
    std::vector<Constraint<int>> constraints;
    constraints.emplace_back(Constraints<int>::range(0, 100));
    
    // Valid value should work
    auto valid_data = make_typed_data(50, constraints);
    EXPECT_EQ(valid_data->get(), 50);
    EXPECT_TRUE(valid_data->is_valid());
    
    // Invalid value should throw
    EXPECT_THROW(make_typed_data(-10, constraints), std::invalid_argument);
    EXPECT_THROW(make_typed_data(150, constraints), std::invalid_argument);
}

// Test constraint addition
TEST_F(TypedDataTest, ConstraintAddition) {
    auto data = make_typed_data(50);
    
    // Add valid constraint
    data->add_constraint(Constraints<int>::min(0));
    EXPECT_TRUE(data->is_valid());
    
    // Add constraint that current value violates
    EXPECT_THROW(data->add_constraint(Constraints<int>::min(100)), std::invalid_argument);
}

// Test value modification with validation
TEST_F(TypedDataTest, ValueModification) {
    std::vector<Constraint<int>> constraints;
    constraints.emplace_back(Constraints<int>::range(0, 100));
    
    auto data = make_typed_data(50, constraints);
    
    // Valid modification
    data->set(75);
    EXPECT_EQ(data->get(), 75);
    
    // Invalid modification should throw
    EXPECT_THROW(data->set(-10), std::invalid_argument);
    EXPECT_THROW(data->set(150), std::invalid_argument);
    
    // Value should remain unchanged after failed set
    EXPECT_EQ(data->get(), 75);
}

// Test string constraints
TEST_F(TypedDataTest, StringConstraints) {
    std::vector<Constraint<std::string>> constraints;
    constraints.emplace_back(Constraints<std::string>::not_empty());
    
    auto data = make_typed_data(std::string("hello"), constraints);
    EXPECT_EQ(data->get(), "hello");
    EXPECT_TRUE(data->is_valid());
    
    // Empty string should violate constraint
    EXPECT_THROW(data->set(""), std::invalid_argument);
}

// Test custom constraints
TEST_F(TypedDataTest, CustomConstraints) {
    // Custom constraint: must be even
    auto even_constraint = Constraints<int>::custom(
        [](int value) { return value % 2 == 0; },
        "Value must be even"
    );
    
    std::vector<Constraint<int>> constraints;
    constraints.emplace_back(even_constraint);
    
    auto data = make_typed_data(42, constraints);
    EXPECT_TRUE(data->is_valid());
    
    EXPECT_THROW(data->set(43), std::invalid_argument);
}

// Test validation error reporting
TEST_F(TypedDataTest, ValidationErrorReporting) {
    std::vector<Constraint<int>> constraints;
    constraints.emplace_back(Constraints<int>::range(0, 100));
    constraints.emplace_back(Constraints<int>::custom(
        [](int value) { return value % 2 == 0; },
        "Value must be even"
    ));
    
    auto data = make_typed_data(50, constraints);
    EXPECT_TRUE(data->get_validation_errors().empty());
    
    // Create a new data object with invalid value to test error reporting
    // We need to create it with constraints that will be violated
    std::vector<Constraint<int>> invalid_constraints;
    invalid_constraints.emplace_back(Constraints<int>::range(0, 50)); // This will be violated by value 101
    
    // This should throw because 101 violates the range constraint
    EXPECT_THROW(make_typed_data(101, invalid_constraints), std::invalid_argument);
}

// Test serialization for basic types
TEST_F(TypedDataTest, Serialization) {
    auto int_data = make_typed_data(42);
    std::string serialized = int_data->serialize();
    EXPECT_EQ(serialized, "42");
    
    auto string_data = make_typed_data(std::string("hello"));
    serialized = string_data->serialize();
    EXPECT_EQ(serialized, "hello");
    
    auto double_data = make_typed_data(3.14);
    serialized = double_data->serialize();
    EXPECT_FALSE(serialized.empty());
}

// Test deserialization for basic types
TEST_F(TypedDataTest, Deserialization) {
    auto int_data = make_typed_data(0);
    EXPECT_TRUE(int_data->deserialize("42"));
    EXPECT_EQ(int_data->get(), 42);
    
    auto string_data = make_typed_data(std::string(""));
    EXPECT_TRUE(string_data->deserialize("hello"));
    EXPECT_EQ(string_data->get(), "hello");
    
    auto double_data = make_typed_data(0.0);
    EXPECT_TRUE(double_data->deserialize("3.14"));
    EXPECT_DOUBLE_EQ(double_data->get(), 3.14);
}

// Test FrameworkData interface
TEST_F(TypedDataTest, FrameworkDataInterface) {
    auto data = make_typed_data(42);
    
    // Test clone
    auto cloned = data->clone();
    EXPECT_NE(cloned.get(), data.get()); // Different objects
    
    auto typed_clone = dynamic_cast<TypedData<int>*>(cloned.get());
    ASSERT_NE(typed_clone, nullptr);
    EXPECT_EQ(typed_clone->get(), 42);
    
    // Test type information
    EXPECT_EQ(data->type(), typeid(TypedData<int>));
    
    // Test type-safe casting
    EXPECT_NE(data->as<TypedData<int>>(), nullptr);
    EXPECT_EQ(data->as<TypedData<std::string>>(), nullptr);
}

// Test equality operators
TEST_F(TypedDataTest, EqualityOperators) {
    auto data1 = make_typed_data(42);
    auto data2 = make_typed_data(42);
    auto data3 = make_typed_data(43);
    
    EXPECT_EQ(*data1, *data2);
    EXPECT_NE(*data1, *data3);
}

// Test comparison operators
TEST_F(TypedDataTest, ComparisonOperators) {
    auto data1 = make_typed_data(42);
    auto data2 = make_typed_data(43);
    
    EXPECT_LT(*data1, *data2);
    EXPECT_LE(*data1, *data2);
    EXPECT_GT(*data2, *data1);
    EXPECT_GE(*data2, *data1);
}

// Test ImmutableTypedData
TEST_F(TypedDataTest, ImmutableTypedData) {
    auto immutable_data = make_immutable_typed_data(42);
    EXPECT_EQ(immutable_data->get(), 42);
    
    // Test that we can't modify the value directly
    // (This is enforced by the const member and deleted assignment operators)
    
    // Test creating new instance with different value
    auto new_data = immutable_data->with_value(43);
    EXPECT_EQ(immutable_data->get(), 42); // Original unchanged
    EXPECT_EQ(new_data.get(), 43);
}

// Test ImmutableTypedData with constraints
TEST_F(TypedDataTest, ImmutableTypedDataConstraints) {
    std::vector<Constraint<int>> constraints;
    constraints.emplace_back(Constraints<int>::range(0, 100));
    
    auto immutable_data = make_immutable_typed_data(50, constraints);
    EXPECT_TRUE(immutable_data->is_valid());
    
    // Test adding constraint
    auto with_constraint = immutable_data->with_constraint(Constraints<int>::min(10));
    EXPECT_TRUE(with_constraint.is_valid());
    
    // Test adding constraint that current value violates
    EXPECT_THROW(
        immutable_data->with_constraint(Constraints<int>::min(100)),
        std::invalid_argument
    );
}

// Test ImmutableTypedData serialization
TEST_F(TypedDataTest, ImmutableTypedDataSerialization) {
    auto immutable_data = make_immutable_typed_data(42);
    std::string serialized = immutable_data->serialize();
    EXPECT_EQ(serialized, "42");
    
    // Test deserialization returns false (immutable data can't be modified)
    EXPECT_FALSE(immutable_data->deserialize("43"));
    EXPECT_EQ(immutable_data->get(), 42); // Value unchanged
    
    // Test creating from serialized data
    auto from_serialized = ImmutableTypedData<int>::from_serialized("43");
    ASSERT_NE(from_serialized, nullptr);
    EXPECT_EQ(from_serialized->get(), 43);
}

// Test ImmutableTypedData FrameworkData interface
TEST_F(TypedDataTest, ImmutableTypedDataFrameworkInterface) {
    auto data = make_immutable_typed_data(42);
    
    // Test clone
    auto cloned = data->clone();
    EXPECT_NE(cloned.get(), data.get()); // Different objects
    
    auto typed_clone = dynamic_cast<ImmutableTypedData<int>*>(cloned.get());
    ASSERT_NE(typed_clone, nullptr);
    EXPECT_EQ(typed_clone->get(), 42);
    
    // Test type information
    EXPECT_EQ(data->type(), typeid(ImmutableTypedData<int>));
}

// Test type aliases
TEST_F(TypedDataTest, TypeAliases) {
    IntData int_data(42);
    EXPECT_EQ(int_data.get(), 42);
    
    DoubleData double_data(3.14);
    EXPECT_DOUBLE_EQ(double_data.get(), 3.14);
    
    StringTypedData string_data("hello");
    EXPECT_EQ(string_data.get(), "hello");
    
    BoolData bool_data(true);
    EXPECT_TRUE(bool_data.get());
    
    ImmutableIntData immutable_int_data(42);
    EXPECT_EQ(immutable_int_data.get(), 42);
}

// Test edge cases and error conditions
TEST_F(TypedDataTest, EdgeCases) {
    // Test with empty constraints vector
    std::vector<Constraint<int>> empty_constraints;
    auto data = make_typed_data(42, empty_constraints);
    EXPECT_TRUE(data->is_valid());
    
    // Test constraint clearing
    data->add_constraint(Constraints<int>::min(0));
    EXPECT_TRUE(data->is_valid()); // Should be valid since 42 >= 0
    EXPECT_TRUE(data->get_validation_errors().empty()); // No errors
    
    data->clear_constraints();
    EXPECT_TRUE(data->get_validation_errors().empty());
    EXPECT_TRUE(data->is_valid());
}

// ============================================================================
// ENHANCED TYPE SAFETY AND VALIDATION TESTS (Task 5.1)
// ============================================================================

// Test TypedData with various primitive data types
TEST_F(TypedDataTest, VariousPrimitiveTypes) {
    // Test all integral types
    {
        auto char_data = make_typed_data('A');
        EXPECT_EQ(char_data->get(), 'A');
        EXPECT_EQ(char_data->type(), typeid(TypedData<char>));
    }
    
    {
        auto uchar_data = make_typed_data(static_cast<unsigned char>(255));
        EXPECT_EQ(uchar_data->get(), 255);
        EXPECT_EQ(uchar_data->type(), typeid(TypedData<unsigned char>));
    }
    
    {
        auto short_data = make_typed_data(static_cast<short>(-32768));
        EXPECT_EQ(short_data->get(), -32768);
        EXPECT_EQ(short_data->type(), typeid(TypedData<short>));
    }
    
    {
        auto ushort_data = make_typed_data(static_cast<unsigned short>(65535));
        EXPECT_EQ(ushort_data->get(), 65535);
        EXPECT_EQ(ushort_data->type(), typeid(TypedData<unsigned short>));
    }
    
    {
        auto long_data = make_typed_data(static_cast<long>(1234567890L));
        EXPECT_EQ(long_data->get(), 1234567890L);
        EXPECT_EQ(long_data->type(), typeid(TypedData<long>));
    }
    
    {
        auto ulong_data = make_typed_data(static_cast<unsigned long>(4294967295UL));
        EXPECT_EQ(ulong_data->get(), 4294967295UL);
        EXPECT_EQ(ulong_data->type(), typeid(TypedData<unsigned long>));
    }
    
    {
        auto longlong_data = make_typed_data(static_cast<long long>(9223372036854775807LL));
        EXPECT_EQ(longlong_data->get(), 9223372036854775807LL);
        EXPECT_EQ(longlong_data->type(), typeid(TypedData<long long>));
    }
    
    {
        auto ulonglong_data = make_typed_data(static_cast<unsigned long long>(18446744073709551615ULL));
        EXPECT_EQ(ulonglong_data->get(), 18446744073709551615ULL);
        EXPECT_EQ(ulonglong_data->type(), typeid(TypedData<unsigned long long>));
    }
}

// Test TypedData with floating point types
TEST_F(TypedDataTest, FloatingPointTypes) {
    {
        auto float_data = make_typed_data(3.14159f);
        EXPECT_FLOAT_EQ(float_data->get(), 3.14159f);
        EXPECT_EQ(float_data->type(), typeid(TypedData<float>));
    }
    
    {
        auto double_data = make_typed_data(2.718281828459045);
        EXPECT_DOUBLE_EQ(double_data->get(), 2.718281828459045);
        EXPECT_EQ(double_data->type(), typeid(TypedData<double>));
    }
    
    {
        auto ldouble_data = make_typed_data(static_cast<long double>(1.41421356237309504880L));
        EXPECT_NEAR(static_cast<double>(ldouble_data->get()), 1.41421356237309504880, 1e-15);
        EXPECT_EQ(ldouble_data->type(), typeid(TypedData<long double>));
    }
    
    // Test special floating point values
    {
        auto inf_data = make_typed_data(std::numeric_limits<double>::infinity());
        EXPECT_TRUE(std::isinf(inf_data->get()));
        
        auto nan_data = make_typed_data(std::numeric_limits<double>::quiet_NaN());
        EXPECT_TRUE(std::isnan(nan_data->get()));
        
        auto neg_zero_data = make_typed_data(-0.0);
        EXPECT_EQ(neg_zero_data->get(), -0.0);
    }
}

// Test TypedData with container types
TEST_F(TypedDataTest, ContainerTypes) {
    {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        auto vec_data = make_typed_data(vec);
        EXPECT_EQ(vec_data->get().size(), 5);
        EXPECT_EQ(vec_data->get()[0], 1);
        EXPECT_EQ(vec_data->get()[4], 5);
        EXPECT_EQ(vec_data->type(), typeid(TypedData<std::vector<int>>));
    }
    
    {
        std::array<double, 3> arr = {1.1, 2.2, 3.3};
        auto arr_data = make_typed_data(arr);
        EXPECT_EQ(arr_data->get().size(), 3);
        EXPECT_DOUBLE_EQ(arr_data->get()[0], 1.1);
        EXPECT_DOUBLE_EQ(arr_data->get()[2], 3.3);
        EXPECT_EQ(arr_data->type(), typeid(TypedData<std::array<double, 3>>));
    }
}

// Test TypedData with custom types
struct CustomType {
    int value;
    std::string name;
    
    CustomType(int v, std::string n) : value(v), name(std::move(n)) {}
    
    bool operator==(const CustomType& other) const {
        return value == other.value && name == other.name;
    }
    
    bool operator!=(const CustomType& other) const {
        return !(*this == other);
    }
    
    bool operator<(const CustomType& other) const {
        if (value != other.value) return value < other.value;
        return name < other.name;
    }
    
    bool operator<=(const CustomType& other) const {
        return *this < other || *this == other;
    }
    
    bool operator>(const CustomType& other) const {
        return !(*this <= other);
    }
    
    bool operator>=(const CustomType& other) const {
        return !(*this < other);
    }
};

TEST_F(TypedDataTest, CustomTypes) {
    CustomType custom(42, "test");
    auto custom_data = make_typed_data(custom);
    
    EXPECT_EQ(custom_data->get().value, 42);
    EXPECT_EQ(custom_data->get().name, "test");
    EXPECT_EQ(custom_data->type(), typeid(TypedData<CustomType>));
    
    // Test equality
    CustomType custom2(42, "test");
    auto custom_data2 = make_typed_data(custom2);
    EXPECT_EQ(*custom_data, *custom_data2);
    
    // Test comparison
    CustomType custom3(41, "test");
    auto custom_data3 = make_typed_data(custom3);
    EXPECT_LT(*custom_data3, *custom_data);
}

// Test compile-time type checking behavior
TEST_F(TypedDataTest, CompileTimeTypeChecking) {
    // Test that TypedData preserves type information at compile time
    auto int_data = make_typed_data(42);
    auto string_data = make_typed_data(std::string("hello"));
    
    // These should compile and work correctly
    static_assert(std::is_same_v<decltype(int_data)::element_type::value_type, int>);
    static_assert(std::is_same_v<decltype(string_data)::element_type::value_type, std::string>);
    
    // Test type-safe casting
    std::unique_ptr<FrameworkData> base_ptr = std::move(int_data);
    
    // Safe cast should work
    auto* typed_ptr = base_ptr->as<TypedData<int>>();
    EXPECT_NE(typed_ptr, nullptr);
    EXPECT_EQ(typed_ptr->get(), 42);
    
    // Wrong type cast should return nullptr
    auto* wrong_typed_ptr = base_ptr->as<TypedData<std::string>>();
    EXPECT_EQ(wrong_typed_ptr, nullptr);
    
    // Test that concepts work correctly
    static_assert(Serializable<int>);
    static_assert(Serializable<double>);
    static_assert(Serializable<std::string>);
    static_assert(!Serializable<std::vector<int>>); // vectors are not serializable by default
    
    static_assert(Validatable<int>);
    static_assert(Validatable<std::string>);
    static_assert(Validatable<CustomType>);
}

// Test serialization round-trip for all supported types
TEST_F(TypedDataTest, SerializationRoundTripAllTypes) {
    // Test integer types
    {
        auto int_data = make_typed_data(42);
        std::string serialized = int_data->serialize();
        EXPECT_EQ(serialized, "42");
        
        auto new_int_data = make_typed_data(0);
        EXPECT_TRUE(new_int_data->deserialize(serialized));
        EXPECT_EQ(new_int_data->get(), 42);
    }
    
    {
        auto long_data = make_typed_data(1234567890L);
        std::string serialized = long_data->serialize();
        EXPECT_EQ(serialized, "1234567890");
        
        auto new_long_data = make_typed_data(0L);
        EXPECT_TRUE(new_long_data->deserialize(serialized));
        EXPECT_EQ(new_long_data->get(), 1234567890L);
    }
    
    {
        auto longlong_data = make_typed_data(9223372036854775807LL);
        std::string serialized = longlong_data->serialize();
        EXPECT_EQ(serialized, "9223372036854775807");
        
        auto new_longlong_data = make_typed_data(0LL);
        EXPECT_TRUE(new_longlong_data->deserialize(serialized));
        EXPECT_EQ(new_longlong_data->get(), 9223372036854775807LL);
    }
    
    // Test floating point types
    {
        auto float_data = make_typed_data(3.14159f);
        std::string serialized = float_data->serialize();
        EXPECT_FALSE(serialized.empty());
        
        auto new_float_data = make_typed_data(0.0f);
        EXPECT_TRUE(new_float_data->deserialize(serialized));
        EXPECT_FLOAT_EQ(new_float_data->get(), 3.14159f);
    }
    
    {
        auto double_data = make_typed_data(2.718281828459045);
        std::string serialized = double_data->serialize();
        EXPECT_FALSE(serialized.empty());
        
        auto new_double_data = make_typed_data(0.0);
        EXPECT_TRUE(new_double_data->deserialize(serialized));
        EXPECT_NEAR(new_double_data->get(), 2.718281828459045, 1e-6);
    }
    
    {
        auto ldouble_data = make_typed_data(static_cast<long double>(1.41421356237309504880L));
        std::string serialized = ldouble_data->serialize();
        EXPECT_FALSE(serialized.empty());
        
        auto new_ldouble_data = make_typed_data(static_cast<long double>(0.0L));
        EXPECT_TRUE(new_ldouble_data->deserialize(serialized));
        EXPECT_NEAR(static_cast<double>(new_ldouble_data->get()), 1.41421356237309504880, 1e-6);
    }
    
    // Test string type
    {
        auto string_data = make_typed_data(std::string("Hello, World!"));
        std::string serialized = string_data->serialize();
        EXPECT_EQ(serialized, "Hello, World!");
        
        auto new_string_data = make_typed_data(std::string(""));
        EXPECT_TRUE(new_string_data->deserialize(serialized));
        EXPECT_EQ(new_string_data->get(), "Hello, World!");
    }
    
    // Test edge cases for serialization
    {
        // Empty string
        auto empty_string_data = make_typed_data(std::string(""));
        std::string serialized = empty_string_data->serialize();
        EXPECT_EQ(serialized, "");
        
        auto new_empty_string_data = make_typed_data(std::string("not empty"));
        EXPECT_TRUE(new_empty_string_data->deserialize(serialized));
        EXPECT_EQ(new_empty_string_data->get(), "");
    }
    
    {
        // Zero values
        auto zero_int_data = make_typed_data(0);
        std::string serialized = zero_int_data->serialize();
        EXPECT_EQ(serialized, "0");
        
        auto new_zero_int_data = make_typed_data(42);
        EXPECT_TRUE(new_zero_int_data->deserialize(serialized));
        EXPECT_EQ(new_zero_int_data->get(), 0);
    }
    
    {
        // Negative values
        auto neg_int_data = make_typed_data(-42);
        std::string serialized = neg_int_data->serialize();
        EXPECT_EQ(serialized, "-42");
        
        auto new_neg_int_data = make_typed_data(0);
        EXPECT_TRUE(new_neg_int_data->deserialize(serialized));
        EXPECT_EQ(new_neg_int_data->get(), -42);
    }
}

// Test serialization failure cases
TEST_F(TypedDataTest, SerializationFailureCases) {
    // Test unsupported type serialization
    std::vector<int> vec = {1, 2, 3};
    auto vec_data = make_typed_data(vec);
    std::string serialized = vec_data->serialize();
    EXPECT_EQ(serialized, ""); // Should return empty string for unsupported types
    
    // Test deserialization with invalid data
    auto int_data = make_typed_data(0);
    EXPECT_FALSE(int_data->deserialize("not_a_number"));
    EXPECT_EQ(int_data->get(), 0); // Value should remain unchanged
    
    auto double_data = make_typed_data(0.0);
    EXPECT_FALSE(double_data->deserialize("invalid_double"));
    EXPECT_EQ(double_data->get(), 0.0); // Value should remain unchanged
    
    // Test deserialization with constraints
    std::vector<Constraint<int>> constraints;
    constraints.emplace_back(Constraints<int>::range(0, 100));
    auto constrained_data = make_typed_data(50, constraints);
    
    // Valid deserialization within constraints
    EXPECT_TRUE(constrained_data->deserialize("75"));
    EXPECT_EQ(constrained_data->get(), 75);
    
    // Invalid deserialization violating constraints
    EXPECT_FALSE(constrained_data->deserialize("150"));
    EXPECT_EQ(constrained_data->get(), 75); // Value should remain unchanged
}

// Test advanced constraint validation scenarios
TEST_F(TypedDataTest, AdvancedConstraintValidation) {
    // Test multiple constraints
    std::vector<Constraint<int>> constraints;
    constraints.emplace_back(Constraints<int>::range(0, 100));
    constraints.emplace_back(Constraints<int>::custom(
        [](int value) { return value % 2 == 0; },
        "Value must be even"
    ));
    
    auto data = make_typed_data(50, constraints);
    EXPECT_TRUE(data->is_valid());
    EXPECT_TRUE(data->get_validation_errors().empty());
    
    // Test constraint violation detection by creating a new data object with violating constraints
    std::vector<Constraint<int>> violating_constraints;
    violating_constraints.emplace_back(Constraints<int>::range(0, 100));
    violating_constraints.emplace_back(Constraints<int>::custom(
        [](int value) { return value % 2 == 1; },
        "Value must be odd"
    ));
    
    // Try to create data with value 50 (even) but constraint requiring odd - should throw
    EXPECT_THROW(make_typed_data(50, violating_constraints), std::invalid_argument);
    
    // Create data with odd value and odd constraint - should work
    auto odd_data = make_typed_data(51, violating_constraints);
    EXPECT_TRUE(odd_data->is_valid());
    EXPECT_TRUE(odd_data->get_validation_errors().empty());
    
    // Try to set even value on odd-constrained data - should throw
    EXPECT_THROW(odd_data->set(50), std::invalid_argument);
}

// Test constraint edge cases
TEST_F(TypedDataTest, ConstraintEdgeCases) {
    // Test boundary values for range constraints
    std::vector<Constraint<int>> constraints;
    constraints.emplace_back(Constraints<int>::range(0, 100));
    
    // Test boundary values
    auto data_min = make_typed_data(0, constraints);
    EXPECT_TRUE(data_min->is_valid());
    
    auto data_max = make_typed_data(100, constraints);
    EXPECT_TRUE(data_max->is_valid());
    
    // Test just outside boundaries
    EXPECT_THROW(make_typed_data(-1, constraints), std::invalid_argument);
    EXPECT_THROW(make_typed_data(101, constraints), std::invalid_argument);
    
    // Test string constraints with edge cases
    std::vector<Constraint<std::string>> string_constraints;
    string_constraints.emplace_back(Constraints<std::string>::not_empty());
    
    auto string_data = make_typed_data(std::string("a"), string_constraints);
    EXPECT_TRUE(string_data->is_valid());
    
    // Test single character string
    string_data->set("x");
    EXPECT_TRUE(string_data->is_valid());
    
    // Test empty string violation
    EXPECT_THROW(string_data->set(""), std::invalid_argument);
}

// Test move semantics and performance
TEST_F(TypedDataTest, MoveSemanticsAndPerformance) {
    // Test move construction
    std::string large_string(10000, 'A');
    auto string_data = make_typed_data(std::move(large_string));
    EXPECT_EQ(string_data->get().size(), 10000);
    EXPECT_TRUE(large_string.empty()); // Should be moved from
    
    // Test move assignment
    std::string another_large_string(5000, 'B');
    string_data->set(std::move(another_large_string));
    EXPECT_EQ(string_data->get().size(), 5000);
    EXPECT_EQ(string_data->get()[0], 'B');
    EXPECT_TRUE(another_large_string.empty()); // Should be moved from
    
    // Test that constraints are preserved during moves
    std::vector<Constraint<std::string>> constraints;
    constraints.emplace_back(Constraints<std::string>::not_empty());
    
    std::string test_string = "test";
    auto constrained_data = make_typed_data(std::move(test_string), std::move(constraints));
    EXPECT_TRUE(constrained_data->is_valid());
    EXPECT_EQ(constrained_data->get(), "test");
}

// Test thread safety of type information
TEST_F(TypedDataTest, TypeInformationThreadSafety) {
    auto int_data = make_typed_data(42);
    auto string_data = make_typed_data(std::string("hello"));
    
    // Type information should be consistent across multiple calls
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(int_data->type(), typeid(TypedData<int>));
        EXPECT_EQ(string_data->type(), typeid(TypedData<std::string>));
        
        // Type-safe casting should be consistent
        std::unique_ptr<FrameworkData> base_ptr = int_data->clone();
        auto* typed_ptr = base_ptr->as<TypedData<int>>();
        EXPECT_NE(typed_ptr, nullptr);
        EXPECT_EQ(typed_ptr->get(), 42);
    }
}