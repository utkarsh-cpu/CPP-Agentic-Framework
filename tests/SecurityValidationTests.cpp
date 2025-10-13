#include <gtest/gtest.h>
#include "../BaseNode/InputValidation.h"
#include "../BaseNode/SecurityUtils.h"
#include "../BaseNode/BaseNode.h"
#include "../BaseNode/BatchData.h"
#include "../BatchNode/VectorData.h"
#include "../BatchFlow/MapData.h"
#include "../Flow/StringData.h"
#include <memory>
#include <limits>

class SecurityValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        Security::SecurityAudit::clear_audit_log();
    }
    
    void TearDown() override {
        Security::SecurityAudit::clear_audit_log();
    }
};

// Test BoundsChecker functionality
TEST_F(SecurityValidationTest, BoundsCheckerTests) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    
    // Valid bounds checking should not throw
    EXPECT_NO_THROW(Security::BoundsChecker::check_bounds(vec, 0, "test"));
    EXPECT_NO_THROW(Security::BoundsChecker::check_bounds(vec, 4, "test"));
    
    // Invalid bounds should throw
    EXPECT_THROW(Security::BoundsChecker::check_bounds(vec, 5, "test"), Security::BoundsCheckException);
    EXPECT_THROW(Security::BoundsChecker::check_bounds(vec, 100, "test"), Security::BoundsCheckException);
    
    // Null pointer checks
    int* null_ptr = nullptr;
    std::shared_ptr<int> null_shared;
    std::unique_ptr<int> null_unique;
    
    EXPECT_THROW(Security::BoundsChecker::check_not_null(null_ptr, "test"), Security::BoundsCheckException);
    EXPECT_THROW(Security::BoundsChecker::check_not_null(null_shared, "test"), Security::BoundsCheckException);
    EXPECT_THROW(Security::BoundsChecker::check_not_null(null_unique, "test"), Security::BoundsCheckException);
    
    // Valid pointers should not throw
    int valid_int = 42;
    auto valid_shared = std::make_shared<int>(42);
    auto valid_unique = std::make_unique<int>(42);
    
    EXPECT_NO_THROW(Security::BoundsChecker::check_not_null(&valid_int, "test"));
    EXPECT_NO_THROW(Security::BoundsChecker::check_not_null(valid_shared, "test"));
    EXPECT_NO_THROW(Security::BoundsChecker::check_not_null(valid_unique, "test"));
}

// Test InputValidator functionality
TEST_F(SecurityValidationTest, InputValidatorTests) {
    // String validation
    EXPECT_NO_THROW(Security::InputValidator::validate_non_empty_string("valid", "test"));
    EXPECT_THROW(Security::InputValidator::validate_non_empty_string("", "test"), Security::InputValidationException);
    
    EXPECT_NO_THROW(Security::InputValidator::validate_string_length("short", 10, "test"));
    EXPECT_THROW(Security::InputValidator::validate_string_length("very long string", 5, "test"), Security::InputValidationException);
    
    // Numeric validation
    EXPECT_NO_THROW(Security::InputValidator::validate_positive(5, "test"));
    EXPECT_THROW(Security::InputValidator::validate_positive(0, "test"), Security::InputValidationException);
    EXPECT_THROW(Security::InputValidator::validate_positive(-1, "test"), Security::InputValidationException);
    
    EXPECT_NO_THROW(Security::InputValidator::validate_non_negative(0, "test"));
    EXPECT_NO_THROW(Security::InputValidator::validate_non_negative(5, "test"));
    EXPECT_THROW(Security::InputValidator::validate_non_negative(-1, "test"), Security::InputValidationException);
    
    EXPECT_NO_THROW(Security::InputValidator::validate_range(5, 1, 10, "test"));
    EXPECT_THROW(Security::InputValidator::validate_range(0, 1, 10, "test"), Security::InputValidationException);
    EXPECT_THROW(Security::InputValidator::validate_range(11, 1, 10, "test"), Security::InputValidationException);
    
    // Container validation
    std::vector<int> empty_vec;
    std::vector<int> valid_vec = {1, 2, 3};
    
    EXPECT_THROW(Security::InputValidator::validate_non_empty_container(empty_vec, "test"), Security::InputValidationException);
    EXPECT_NO_THROW(Security::InputValidator::validate_non_empty_container(valid_vec, "test"));
    
    EXPECT_NO_THROW(Security::InputValidator::validate_container_size(valid_vec, 10, "test"));
    EXPECT_THROW(Security::InputValidator::validate_container_size(valid_vec, 2, "test"), Security::InputValidationException);
}

// Test SafeCaster functionality
TEST_F(SecurityValidationTest, SafeCasterTests) {
    struct Base { virtual ~Base() = default; };
    struct Derived : public Base {};
    struct Other : public Base {};
    
    auto base_ptr = std::make_unique<Base>();
    auto derived_ptr = std::make_unique<Derived>();
    auto other_ptr = std::make_unique<Other>();
    
    // Valid casts should work
    EXPECT_NO_THROW(Security::SafeCaster::safe_cast<Base>(derived_ptr.get(), "test"));
    
    // Invalid casts should throw
    EXPECT_THROW(Security::SafeCaster::safe_cast<Derived>(base_ptr.get(), "test"), Security::UnsafeCastException);
    EXPECT_THROW(Security::SafeCaster::safe_cast<Other>(derived_ptr.get(), "test"), Security::UnsafeCastException);
    
    // Null pointer casts should throw
    Base* null_base = nullptr;
    EXPECT_THROW(Security::SafeCaster::safe_cast<Derived>(null_base, "test"), Security::UnsafeCastException);
    
    // Optional casts should return nullptr on failure
    EXPECT_EQ(Security::SafeCaster::optional_cast<Derived>(base_ptr.get()), nullptr);
    EXPECT_NE(Security::SafeCaster::optional_cast<Base>(derived_ptr.get()), nullptr);
    
    // Numeric casts
    EXPECT_NO_THROW(Security::SafeCaster::safe_numeric_cast<int>(42L, "test"));
    EXPECT_THROW(Security::SafeCaster::safe_numeric_cast<char>(1000, "test"), Security::UnsafeCastException);
}

// Test OverflowProtection functionality
TEST_F(SecurityValidationTest, OverflowProtectionTests) {
    // Safe addition
    EXPECT_EQ(Security::OverflowProtection::safe_add(5, 3, "test"), 8);
    EXPECT_THROW(Security::OverflowProtection::safe_add(std::numeric_limits<int>::max(), 1, "test"), 
                 Security::IntegerOverflowException);
    
    // Safe subtraction
    EXPECT_EQ(Security::OverflowProtection::safe_subtract(10, 3, "test"), 7);
    EXPECT_THROW(Security::OverflowProtection::safe_subtract(std::numeric_limits<int>::lowest(), 1, "test"), 
                 Security::IntegerOverflowException);
    
    // Safe multiplication
    EXPECT_EQ(Security::OverflowProtection::safe_multiply(5, 3, "test"), 15);
    EXPECT_THROW(Security::OverflowProtection::safe_multiply(std::numeric_limits<int>::max(), 2, "test"), 
                 Security::IntegerOverflowException);
    
    // Safe division
    EXPECT_EQ(Security::OverflowProtection::safe_divide(15, 3, "test"), 5);
    EXPECT_THROW(Security::OverflowProtection::safe_divide(10, 0, "test"), Security::IntegerOverflowException);
    EXPECT_THROW(Security::OverflowProtection::safe_divide(std::numeric_limits<int>::lowest(), -1, "test"), 
                 Security::IntegerOverflowException);
    
    // Safe increment/decrement
    int value = 5;
    EXPECT_EQ(Security::OverflowProtection::safe_increment(value, "test"), 6);
    EXPECT_EQ(value, 6);
    
    value = std::numeric_limits<int>::max();
    EXPECT_THROW(Security::OverflowProtection::safe_increment(value, "test"), Security::IntegerOverflowException);
    
    value = 5;
    EXPECT_EQ(Security::OverflowProtection::safe_decrement(value, "test"), 4);
    EXPECT_EQ(value, 4);
    
    value = std::numeric_limits<int>::lowest();
    EXPECT_THROW(Security::OverflowProtection::safe_decrement(value, "test"), Security::IntegerOverflowException);
}

// Test BaseNode security enhancements
TEST_F(SecurityValidationTest, BaseNodeSecurityTests) {
    auto node = std::make_shared<BaseNode>();
    auto data = std::make_shared<StringData>("test");
    
    // Valid parameter operations should work
    EXPECT_NO_THROW(node->setParamSafe("key", data));
    EXPECT_NO_THROW(node->getParamSafe("key"));
    
    // Invalid operations should throw
    EXPECT_THROW(node->setParamSafe("", data), Security::InputValidationException);
    EXPECT_THROW(node->setParamSafe("key", nullptr), Security::BoundsCheckException);
    EXPECT_THROW(node->getParamSafe("nonexistent"), Security::InputValidationException);
    
    // Test parameter validation
    std::map<std::string, std::shared_ptr<FrameworkData>> params;
    params["valid_key"] = data;
    params[""] = data; // Invalid empty key
    
    EXPECT_THROW(node->setParam(params), Security::InputValidationException);
    
    // Test successor validation
    auto successor = std::make_shared<BaseNode>();
    EXPECT_NO_THROW(node->next(successor, "action"));
    EXPECT_THROW(node->next(nullptr, "action"), Security::BoundsCheckException);
}

// Test BatchData security enhancements
TEST_F(SecurityValidationTest, BatchDataSecurityTests) {
    BatchData batch;
    auto data = std::make_shared<StringData>("test");
    
    // Valid operations should work
    EXPECT_NO_THROW(batch.add_item(data));
    EXPECT_NO_THROW(batch.at(0));
    
    // Invalid operations should throw
    EXPECT_THROW(batch.add_item(nullptr), Security::BoundsCheckException);
    EXPECT_THROW(batch.at(1), Security::BoundsCheckException);
    
    // Test empty batch operations
    BatchData empty_batch;
    EXPECT_THROW(empty_batch.front(), Security::InputValidationException);
    EXPECT_THROW(empty_batch.back(), Security::InputValidationException);
    EXPECT_THROW(empty_batch.pop_back(), Security::InputValidationException);
}

// Test VectorData security enhancements
TEST_F(SecurityValidationTest, VectorDataSecurityTests) {
    VectorData vector_data;
    auto data = std::make_shared<StringData>("test");
    
    // Valid operations should work
    EXPECT_NO_THROW(vector_data.push_back(data));
    EXPECT_NO_THROW(vector_data.at(0));
    
    // Invalid operations should throw
    EXPECT_THROW(vector_data.push_back(nullptr), Security::BoundsCheckException);
    EXPECT_THROW(vector_data.at(1), Security::BoundsCheckException);
}

// Test MapData security enhancements
TEST_F(SecurityValidationTest, MapDataSecurityTests) {
    MapData map_data;
    auto data = std::make_shared<StringData>("test");
    
    // Valid operations should work
    EXPECT_NO_THROW(map_data.insert("key", data));
    EXPECT_NO_THROW(map_data.at("key"));
    
    // Invalid operations should throw
    EXPECT_THROW(map_data.insert("", data), Security::InputValidationException);
    EXPECT_THROW(map_data.insert("key", nullptr), Security::BoundsCheckException);
    EXPECT_THROW(map_data.at(""), Security::InputValidationException);
    EXPECT_THROW(map_data.at("nonexistent"), Security::InputValidationException);
}

// Test SecureString functionality
TEST_F(SecurityValidationTest, SecureStringTests) {
    std::string test_str = "Hello, World!";
    
    // Valid operations should work
    EXPECT_NO_THROW(Security::SecureString::safe_substr(test_str, 0, 5));
    EXPECT_EQ(Security::SecureString::safe_substr(test_str, 0, 5), "Hello");
    EXPECT_NO_THROW(Security::SecureString::safe_at(test_str, 0));
    EXPECT_EQ(Security::SecureString::safe_at(test_str, 0), 'H');
    
    // Invalid operations should throw
    EXPECT_THROW(Security::SecureString::safe_substr(test_str, 20, 5), Security::BoundsCheckException);
    EXPECT_THROW(Security::SecureString::safe_at(test_str, 20), Security::BoundsCheckException);
    
    // Test safe concatenation
    std::string a = "Hello";
    std::string b = " World";
    EXPECT_NO_THROW(Security::SecureString::safe_concat(a, b, 100));
    EXPECT_EQ(Security::SecureString::safe_concat(a, b, 100), "Hello World");
    EXPECT_THROW(Security::SecureString::safe_concat(a, b, 5), Security::InputValidationException);
}

// Test SecureMemory functionality
TEST_F(SecurityValidationTest, SecureMemoryTests) {
    // Valid allocation should work
    EXPECT_NO_THROW(Security::SecureMemory::safe_allocate_array<int>(100, 1000));
    
    // Invalid allocation should throw
    EXPECT_THROW(Security::SecureMemory::safe_allocate_array<int>(0, 1000), Security::InputValidationException);
    EXPECT_THROW(Security::SecureMemory::safe_allocate_array<int>(2000, 1000), Security::InputValidationException);
    
    // Test safe copy
    int source[] = {1, 2, 3, 4, 5};
    int dest[10];
    
    EXPECT_NO_THROW(Security::SecureMemory::safe_copy(source, dest, 5, 10));
    EXPECT_THROW(Security::SecureMemory::safe_copy(source, dest, 15, 10), Security::BoundsCheckException);
    
    int* null_source = nullptr;
    EXPECT_THROW(Security::SecureMemory::safe_copy(null_source, dest, 5, 10), Security::BoundsCheckException);
}

// Test SecureFilePath functionality
TEST_F(SecurityValidationTest, SecureFilePathTests) {
    // Valid paths should work
    EXPECT_NO_THROW(Security::SecureFilePath::validate_path("valid/path/file.txt"));
    EXPECT_NO_THROW(Security::SecureFilePath::normalize_path("valid/path/file.txt"));
    
    // Invalid paths should throw
    EXPECT_THROW(Security::SecureFilePath::validate_path(""), Security::InputValidationException);
    EXPECT_THROW(Security::SecureFilePath::validate_path("../../../etc/passwd"), Security::SecurityException);
    
    // Create a string with null bytes explicitly
    std::string path_with_nulls = "path";
    path_with_nulls += '\0';
    path_with_nulls += "with";
    path_with_nulls += '\0';
    path_with_nulls += "nulls";
    EXPECT_THROW(Security::SecureFilePath::validate_path(path_with_nulls), Security::SecurityException);
    
    EXPECT_THROW(Security::SecureFilePath::validate_path("path<with>dangerous:chars"), Security::SecurityException);
}

// Test SecureNumeric functionality
TEST_F(SecurityValidationTest, SecureNumericTests) {
    // Safe division with precision
    EXPECT_NO_THROW(Security::SecureNumeric::safe_divide_with_precision(10.0, 2.0, 0.0, 100.0));
    EXPECT_EQ(Security::SecureNumeric::safe_divide_with_precision(10.0, 2.0, 0.0, 100.0), 5.0);
    EXPECT_THROW(Security::SecureNumeric::safe_divide_with_precision(10.0, 0.0, 0.0, 100.0), 
                 Security::IntegerOverflowException);
    
    // Safe modulo
    EXPECT_EQ(Security::SecureNumeric::safe_modulo(10, 3), 1);
    EXPECT_THROW(Security::SecureNumeric::safe_modulo(10, 0), Security::IntegerOverflowException);
    
    // Safe power
    EXPECT_EQ(Security::SecureNumeric::safe_power(2, 3), 8);
    EXPECT_EQ(Security::SecureNumeric::safe_power(5, 0), 1);
    EXPECT_THROW(Security::SecureNumeric::safe_power(2, -1), Security::InputValidationException);
    EXPECT_THROW(Security::SecureNumeric::safe_power(2, 100), Security::InputValidationException);
}

// Test SecurityAudit functionality
TEST_F(SecurityValidationTest, SecurityAuditTests) {
    Security::SecurityAudit::clear_audit_log();
    
    Security::SecurityAudit::log_security_event("Test event", "Test context");
    
    const auto& log = Security::SecurityAudit::get_audit_log();
    EXPECT_EQ(log.size(), 1);
    EXPECT_TRUE(log[0].find("Test event") != std::string::npos);
    EXPECT_TRUE(log[0].find("Test context") != std::string::npos);
}

// Test SecurityContext RAII functionality
TEST_F(SecurityValidationTest, SecurityContextTests) {
    Security::SecurityAudit::clear_audit_log();
    
    {
        Security::SecurityContext ctx("Test operation");
        // Context should log start
    }
    // Context should log completion when destroyed
    
    const auto& log = Security::SecurityAudit::get_audit_log();
    EXPECT_EQ(log.size(), 2);
    EXPECT_TRUE(log[0].find("Starting operation: Test operation") != std::string::npos);
    EXPECT_TRUE(log[1].find("Completed operation: Test operation") != std::string::npos);
}

// Additional bounds checking tests for edge cases
TEST_F(SecurityValidationTest, ExtensiveBoundsCheckingTests) {
    // Test with maximum size_t values to check for integer overflow
    std::vector<int> small_vec = {1, 2, 3};
    
    // Test with extremely large indices
    EXPECT_THROW(Security::BoundsChecker::check_bounds(small_vec, SIZE_MAX, "overflow test"), 
                 Security::BoundsCheckException);
    EXPECT_THROW(Security::BoundsChecker::check_bounds(small_vec, SIZE_MAX - 1, "overflow test"), 
                 Security::BoundsCheckException);
    
    // Test string bounds with edge cases
    std::string test_str = "test";
    EXPECT_THROW(Security::BoundsChecker::check_string_bounds(test_str, SIZE_MAX, 1, "overflow test"), 
                 Security::BoundsCheckException);
    // Note: SIZE_MAX length is treated as npos, so this test is adjusted
    EXPECT_THROW(Security::BoundsChecker::check_string_bounds(test_str, 2, 10, "overflow test"), 
                 Security::BoundsCheckException);
    
    // Test range bounds with inverted ranges
    EXPECT_THROW(Security::BoundsChecker::check_range(5, 10, 1, "inverted range"), 
                 Security::BoundsCheckException);
    
    // Test with negative values for unsigned types
    EXPECT_THROW(Security::BoundsChecker::check_range(static_cast<size_t>(-1), size_t(0), size_t(10), "negative unsigned"), 
                 Security::BoundsCheckException);
}

// Comprehensive malformed data input validation tests
TEST_F(SecurityValidationTest, MalformedDataValidationTests) {
    // Test extremely long strings
    std::string very_long_string(10000, 'x');
    EXPECT_THROW(Security::InputValidator::validate_string_length(very_long_string, 100, "long string test"), 
                 Security::InputValidationException);
    
    // Test strings with control characters
    std::string control_char_string = "test\x01\x02\x03";
    // Note: Current implementation doesn't check for control chars, but we test the framework
    EXPECT_NO_THROW(Security::InputValidator::validate_non_empty_string(control_char_string, "control chars"));
    
    // Test strings with null terminators in the middle
    std::string null_embedded_string = "test";
    null_embedded_string += '\0';
    null_embedded_string += "more";
    EXPECT_NO_THROW(Security::InputValidator::validate_non_empty_string(null_embedded_string, "null embedded"));
    
    // Test numeric edge cases
    EXPECT_THROW(Security::InputValidator::validate_positive(std::numeric_limits<int>::lowest(), "min int"), 
                 Security::InputValidationException);
    EXPECT_THROW(Security::InputValidator::validate_non_negative(std::numeric_limits<int>::lowest(), "min int"), 
                 Security::InputValidationException);
    
    // Test floating point edge cases
    EXPECT_THROW(Security::InputValidator::validate_positive(0.0, "zero double"), 
                 Security::InputValidationException);
    EXPECT_THROW(Security::InputValidator::validate_positive(-0.0, "negative zero"), 
                 Security::InputValidationException);
    
    // Test container size limits with edge cases
    std::vector<int> max_size_vec;
    max_size_vec.resize(5); // Make it have some size
    // Test with a max size smaller than current size
    EXPECT_THROW(Security::InputValidator::validate_container_size(max_size_vec, 2, "small max size"), 
                 Security::InputValidationException);
}

// Advanced unsafe casting tests
TEST_F(SecurityValidationTest, AdvancedUnsafeCastingTests) {
    // Test casting between unrelated class hierarchies
    struct BaseA { virtual ~BaseA() = default; };
    struct DerivedA : public BaseA {};
    struct BaseB { virtual ~BaseB() = default; };
    struct DerivedB : public BaseB {};
    
    auto derived_a = std::make_unique<DerivedA>();
    auto derived_b = std::make_unique<DerivedB>();
    
    // Test cross-hierarchy casting (should fail)
    EXPECT_THROW(Security::SafeCaster::safe_cast<DerivedB>(derived_a.get(), "cross hierarchy"), 
                 Security::UnsafeCastException);
    EXPECT_THROW(Security::SafeCaster::safe_cast<BaseB>(derived_a.get(), "cross hierarchy base"), 
                 Security::UnsafeCastException);
    
    // Test casting with multiple inheritance
    struct MultiBase1 { virtual ~MultiBase1() = default; };
    struct MultiBase2 { virtual ~MultiBase2() = default; };
    struct MultiDerived : public MultiBase1, public MultiBase2 {};
    
    auto multi_derived = std::make_unique<MultiDerived>();
    MultiBase1* base1_ptr = multi_derived.get();
    
    // This should work
    EXPECT_NO_THROW(Security::SafeCaster::safe_cast<MultiDerived>(base1_ptr, "multi inheritance"));
    EXPECT_NO_THROW(Security::SafeCaster::safe_cast<MultiBase2>(multi_derived.get(), "multi inheritance"));
    
    // Test numeric casting edge cases
    EXPECT_THROW(Security::SafeCaster::safe_numeric_cast<char>(1000, "char overflow"), 
                 Security::UnsafeCastException);
    EXPECT_THROW(Security::SafeCaster::safe_numeric_cast<short>(std::numeric_limits<int>::max(), "short overflow"), 
                 Security::UnsafeCastException);
    
    // Test signed to unsigned conversion with negative values
    EXPECT_THROW(Security::SafeCaster::safe_numeric_cast<unsigned char>(-1, "negative to unsigned"), 
                 Security::UnsafeCastException);
    
    // Test integer to integer casting with actual overflow conditions
    long long large_long = static_cast<long long>(std::numeric_limits<int>::max()) + 1;
    EXPECT_THROW(Security::SafeCaster::safe_numeric_cast<int>(large_long, "long to int overflow"), 
                 Security::UnsafeCastException);
    long long small_long = static_cast<long long>(std::numeric_limits<int>::lowest()) - 1;
    EXPECT_THROW(Security::SafeCaster::safe_numeric_cast<int>(small_long, "long to int underflow"), 
                 Security::UnsafeCastException);
    
    // Test shared_ptr casting with null pointers
    std::shared_ptr<BaseA> null_shared_a;
    EXPECT_THROW(Security::SafeCaster::safe_cast<DerivedA>(null_shared_a, "null shared_ptr"), 
                 Security::UnsafeCastException);
    
    // Test optional casting returns nullptr for invalid casts
    EXPECT_EQ(Security::SafeCaster::optional_cast<DerivedB>(derived_a.get()), nullptr);
    EXPECT_EQ(Security::SafeCaster::optional_cast<BaseB>(derived_a.get()), nullptr);
}

// Test security vulnerabilities in container operations
TEST_F(SecurityValidationTest, ContainerSecurityVulnerabilityTests) {
    // Test BatchData with malicious input
    BatchData batch;
    
    // Try to add too many items to trigger resource exhaustion protection
    for (int i = 0; i < 100; ++i) {
        auto data = std::make_shared<StringData>("item_" + std::to_string(i));
        EXPECT_NO_THROW(batch.add_item(data));
    }
    
    // Test accessing beyond container bounds with various edge cases
    EXPECT_THROW(batch.at(batch.size()), Security::BoundsCheckException);
    EXPECT_THROW(batch.at(batch.size() + 1), Security::BoundsCheckException);
    EXPECT_THROW(batch.at(SIZE_MAX), Security::BoundsCheckException);
    
    // Test VectorData security
    VectorData vector_data;
    auto test_data = std::make_shared<StringData>("test");
    vector_data.push_back(test_data);
    
    EXPECT_THROW(vector_data.at(1), Security::BoundsCheckException);
    EXPECT_THROW(vector_data.at(SIZE_MAX), Security::BoundsCheckException);
    
    // Test MapData security with various malformed keys
    MapData map_data;
    
    // Test with empty key
    EXPECT_THROW(map_data.insert("", test_data), Security::InputValidationException);
    EXPECT_THROW(map_data.at(""), Security::InputValidationException);
    
    // Test with very long key
    std::string long_key(1000, 'x');
    EXPECT_THROW(map_data.insert(long_key, test_data), Security::InputValidationException);
    
    // Test accessing non-existent key
    map_data.insert("valid_key", test_data);
    EXPECT_THROW(map_data.at("nonexistent_key"), Security::InputValidationException);
}

// Test integer overflow protection edge cases
TEST_F(SecurityValidationTest, IntegerOverflowEdgeCaseTests) {
    // Test addition near limits
    EXPECT_NO_THROW(Security::OverflowProtection::safe_add(std::numeric_limits<int>::max() - 1, 1, "near max"));
    EXPECT_THROW(Security::OverflowProtection::safe_add(std::numeric_limits<int>::max(), 1, "at max"), 
                 Security::IntegerOverflowException);
    
    // Test subtraction near limits
    EXPECT_NO_THROW(Security::OverflowProtection::safe_subtract(std::numeric_limits<int>::lowest() + 1, 1, "near min"));
    EXPECT_THROW(Security::OverflowProtection::safe_subtract(std::numeric_limits<int>::lowest(), 1, "at min"), 
                 Security::IntegerOverflowException);
    
    // Test multiplication with large numbers
    EXPECT_THROW(Security::OverflowProtection::safe_multiply(std::numeric_limits<int>::max() / 2 + 1, 2, "mult overflow"), 
                 Security::IntegerOverflowException);
    
    // Test division edge cases
    EXPECT_THROW(Security::OverflowProtection::safe_divide(std::numeric_limits<int>::lowest(), -1, "div overflow"), 
                 Security::IntegerOverflowException);
    EXPECT_THROW(Security::OverflowProtection::safe_divide(10, 0, "div by zero"), 
                 Security::IntegerOverflowException);
    
    // Test increment/decrement at limits
    int max_val = std::numeric_limits<int>::max();
    int min_val = std::numeric_limits<int>::lowest();
    
    EXPECT_THROW(Security::OverflowProtection::safe_increment(max_val, "inc at max"), 
                 Security::IntegerOverflowException);
    EXPECT_THROW(Security::OverflowProtection::safe_decrement(min_val, "dec at min"), 
                 Security::IntegerOverflowException);
}

// Test memory security vulnerabilities
TEST_F(SecurityValidationTest, MemorySecurityTests) {
    // Test safe memory allocation limits
    EXPECT_THROW(Security::SecureMemory::safe_allocate_array<int>(0, 1000), 
                 Security::InputValidationException);
    EXPECT_THROW(Security::SecureMemory::safe_allocate_array<int>(SIZE_MAX, 1000), 
                 Security::InputValidationException);
    
    // Test safe memory copy with edge cases
    int source[5] = {1, 2, 3, 4, 5};
    int dest[3];
    
    // Try to copy more data than destination can hold
    EXPECT_THROW(Security::SecureMemory::safe_copy(source, dest, 5, 3), 
                 Security::BoundsCheckException);
    
    // Test with null pointers
    int* null_source = nullptr;
    int* null_dest = nullptr;
    EXPECT_THROW(Security::SecureMemory::safe_copy(null_source, dest, 1, 3), 
                 Security::BoundsCheckException);
    EXPECT_THROW(Security::SecureMemory::safe_copy(source, null_dest, 1, 3), 
                 Security::BoundsCheckException);
}

// Test file path security vulnerabilities
TEST_F(SecurityValidationTest, FilePathSecurityTests) {
    // Test various path traversal attempts
    EXPECT_THROW(Security::SecureFilePath::validate_path("../../../etc/passwd"), 
                 Security::SecurityException);
    EXPECT_THROW(Security::SecureFilePath::validate_path("..\\..\\..\\windows\\system32"), 
                 Security::SecurityException);
    EXPECT_THROW(Security::SecureFilePath::validate_path("legitimate/../../../etc/passwd"), 
                 Security::SecurityException);
    
    // Test paths with dangerous characters
    EXPECT_THROW(Security::SecureFilePath::validate_path("file<script>"), 
                 Security::SecurityException);
    EXPECT_THROW(Security::SecureFilePath::validate_path("file|command"), 
                 Security::SecurityException);
    EXPECT_THROW(Security::SecureFilePath::validate_path("file\"injection"), 
                 Security::SecurityException);
    
    // Test extremely long paths
    std::string long_path(5000, 'x');
    EXPECT_THROW(Security::SecureFilePath::validate_path(long_path), 
                 Security::InputValidationException);
    
    // Test paths with null bytes (potential security bypass)
    std::string null_path = "legitimate_file";
    null_path += '\0';
    null_path += "../../../etc/passwd";
    EXPECT_THROW(Security::SecureFilePath::validate_path(null_path), 
                 Security::SecurityException);
}

// Test BaseNode security with malicious inputs
TEST_F(SecurityValidationTest, BaseNodeMaliciousInputTests) {
    auto node = std::make_shared<BaseNode>();
    auto valid_data = std::make_shared<StringData>("test");
    
    // Test parameter key validation
    EXPECT_THROW(node->setParamSafe("", valid_data), Security::InputValidationException);
    
    std::string long_key(300, 'x'); // Exceeds 256 char limit
    EXPECT_THROW(node->setParamSafe(long_key, valid_data), Security::InputValidationException);
    
    // Test null parameter value
    EXPECT_THROW(node->setParamSafe("valid_key", nullptr), Security::BoundsCheckException);
    
    // Test accessing non-existent parameter
    EXPECT_THROW(node->getParamSafe("nonexistent"), Security::InputValidationException);
    
    // Test successor validation
    EXPECT_THROW(node->next(nullptr, "action"), Security::BoundsCheckException);
    
    std::string long_action(300, 'x');
    EXPECT_THROW(node->next(node, long_action), Security::InputValidationException);
}

// Integration test combining multiple security features
TEST_F(SecurityValidationTest, IntegrationSecurityTest) {
    SECURITY_CONTEXT("Integration test");
    
    auto node = std::make_shared<BaseNode>();
    auto batch = std::make_unique<BatchData>();
    auto vector_data = std::make_unique<VectorData>();
    auto map_data = std::make_unique<MapData>();
    
    // Create secure data
    auto string_data = std::make_shared<StringData>("secure_test_data");
    
    // Test secure operations across different components
    EXPECT_NO_THROW(node->setParamSafe("test_param", string_data));
    EXPECT_NO_THROW(batch->add_item(string_data));
    EXPECT_NO_THROW(vector_data->push_back(string_data));
    EXPECT_NO_THROW(map_data->insert("test_key", string_data));
    
    // Verify data integrity
    EXPECT_EQ(node->getParamSafe("test_param"), string_data);
    EXPECT_EQ(batch->at(0), string_data);
    EXPECT_EQ(vector_data->at(0), string_data);
    EXPECT_EQ(map_data->at("test_key"), string_data);
    
    // Test that security audit logged the operation
    const auto& log = Security::SecurityAudit::get_audit_log();
    EXPECT_GE(log.size(), 1); // At least the start operation
}

// Comprehensive security stress test
TEST_F(SecurityValidationTest, SecurityStressTest) {
    SECURITY_CONTEXT("Security stress test");
    
    // Test rapid allocation and deallocation to check for memory leaks
    for (int i = 0; i < 1000; ++i) {
        auto batch = std::make_unique<BatchData>();
        auto data = std::make_shared<StringData>("stress_test_" + std::to_string(i));
        batch->add_item(data);
        
        // Test bounds checking under stress
        EXPECT_NO_THROW(batch->at(0));
        EXPECT_THROW(batch->at(1), Security::BoundsCheckException);
    }
    
    // Test concurrent access patterns (basic test)
    auto shared_batch = std::make_shared<BatchData>();
    for (int i = 0; i < 100; ++i) {
        auto data = std::make_shared<StringData>("concurrent_" + std::to_string(i));
        shared_batch->add_item(data);
    }
    
    // Verify all items are accessible
    for (size_t i = 0; i < shared_batch->size(); ++i) {
        EXPECT_NO_THROW(shared_batch->at(i));
    }
    
    // Test that out-of-bounds access still fails
    EXPECT_THROW(shared_batch->at(shared_batch->size()), Security::BoundsCheckException);
}