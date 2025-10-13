#include "../BaseNode/TypedData.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== TypedData Example ===" << std::endl;
    
    // Basic usage
    std::cout << "\n1. Basic TypedData usage:" << std::endl;
    auto int_data = make_typed_data(42);
    std::cout << "Integer value: " << int_data->get() << std::endl;
    
    auto string_data = make_typed_data(std::string("Hello, World!"));
    std::cout << "String value: " << string_data->get() << std::endl;
    
    // Constraints
    std::cout << "\n2. TypedData with constraints:" << std::endl;
    std::vector<Constraint<int>> constraints;
    constraints.emplace_back(Constraints<int>::range(0, 100));
    constraints.emplace_back(Constraints<int>::custom(
        [](int value) { return value % 2 == 0; },
        "Value must be even"
    ));
    
    auto constrained_data = make_typed_data(50, constraints);
    std::cout << "Constrained value: " << constrained_data->get() << std::endl;
    std::cout << "Is valid: " << (constrained_data->is_valid() ? "Yes" : "No") << std::endl;
    
    // Try to set invalid value
    try {
        constrained_data->set(51); // This should fail (odd number)
    } catch (const std::exception& e) {
        std::cout << "Constraint violation caught: " << e.what() << std::endl;
    }
    
    // Serialization
    std::cout << "\n3. Serialization:" << std::endl;
    std::cout << "Serialized int: " << int_data->serialize() << std::endl;
    std::cout << "Serialized string: " << string_data->serialize() << std::endl;
    
    // Immutable data
    std::cout << "\n4. Immutable TypedData:" << std::endl;
    auto immutable_data = make_immutable_typed_data(42);
    std::cout << "Immutable value: " << immutable_data->get() << std::endl;
    
    auto new_immutable = immutable_data->with_value(43);
    std::cout << "Original immutable: " << immutable_data->get() << std::endl;
    std::cout << "New immutable: " << new_immutable.get() << std::endl;
    
    // Type safety
    std::cout << "\n5. Type safety:" << std::endl;
    std::unique_ptr<FrameworkData> framework_data = make_typed_data(123);
    
    // Safe casting
    if (auto typed_ptr = framework_data->as<TypedData<int>>()) {
        std::cout << "Successfully cast to TypedData<int>: " << typed_ptr->get() << std::endl;
    }
    
    if (auto wrong_cast = framework_data->as<TypedData<std::string>>()) {
        std::cout << "This shouldn't print" << std::endl;
    } else {
        std::cout << "Correctly failed to cast to TypedData<string>" << std::endl;
    }
    
    // Type aliases
    std::cout << "\n6. Type aliases:" << std::endl;
    IntData int_alias(100);
    StringTypedData string_alias("Type alias example");
    BoolData bool_alias(true);
    
    std::cout << "IntData: " << int_alias.get() << std::endl;
    std::cout << "StringTypedData: " << string_alias.get() << std::endl;
    std::cout << "BoolData: " << (bool_alias.get() ? "true" : "false") << std::endl;
    
    std::cout << "\n=== Example completed successfully ===" << std::endl;
    return 0;
}