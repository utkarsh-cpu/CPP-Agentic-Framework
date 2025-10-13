//
// Created by Utkarsh Kaushik on 13/10/25.
//
//
// Created by AI Assistant
//

#include <gtest/gtest.h>
#include <memory>
#include "../BaseNode/ConditionalTransition.h"
#include "../BaseNode/BaseNode.h"

// Mock node class for testing
class MockNode : public BaseNode {
public:
    explicit MockNode(const std::string& /* name */) : BaseNode() {}

    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override {
        (void)shared; // Suppress unused parameter warning
        return nullptr;
    }
};

class ConditionalTransitionTest : public ::testing::Test {
protected:
    std::shared_ptr<BaseNode> sourceNode;
    std::shared_ptr<BaseNode> targetNode;
    std::string testAction;

    void SetUp() override {
        sourceNode = std::make_shared<MockNode>("source");
        targetNode = std::make_shared<MockNode>("target");
        testAction = "test_action";
    }

    void TearDown() override {
        sourceNode.reset();
        targetNode.reset();
    }
};

// Test constructor
TEST_F(ConditionalTransitionTest, ConstructorInitializesMembers) {
    ConditionalTransition transition(sourceNode, testAction);

    EXPECT_EQ(transition.src, sourceNode);
    EXPECT_EQ(transition.action, testAction);
}

// Test constructor with empty action
TEST_F(ConditionalTransitionTest, ConstructorWithEmptyAction) {
    ConditionalTransition transition(sourceNode, "");

    EXPECT_EQ(transition.src, sourceNode);
    EXPECT_EQ(transition.action, "");
}

// Test operator>> creates connection
TEST_F(ConditionalTransitionTest, OperatorCreatesConnection) {
    ConditionalTransition transition(sourceNode, testAction);

    auto result = transition >> targetNode;

    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result, sourceNode);
}

// Test operator>> with multiple transitions
TEST_F(ConditionalTransitionTest, MultipleTransitionsFromSameSource) {
    std::string action1 = "action1";
    std::string action2 = "action2";
    auto target1 = std::make_shared<MockNode>("target1");
    auto target2 = std::make_shared<MockNode>("target2");

    ConditionalTransition transition1(sourceNode, action1);
    ConditionalTransition transition2(sourceNode, action2);

    auto result1 = transition1 >> target1;
    auto result2 = transition2 >> target2;

    EXPECT_EQ(result1, sourceNode);
    EXPECT_EQ(result2, sourceNode);
}

// Test copy semantics
TEST_F(ConditionalTransitionTest, CopyTransition) {
    ConditionalTransition transition1(sourceNode, testAction);
    ConditionalTransition transition2 = transition1;

    EXPECT_EQ(transition2.src, sourceNode);
    EXPECT_EQ(transition2.action, testAction);
}

// Test with different action strings
TEST_F(ConditionalTransitionTest, DifferentActionStrings) {
    std::vector<std::string> actions = {
        "action1",
        "action_with_underscore",
        "123numeric",
        "special!@#",
        "very_long_action_name_for_testing"
    };

    for (const auto& action : actions) {
        ConditionalTransition transition(sourceNode, action);
        EXPECT_EQ(transition.action, action);
    }
}

// Test operator>> returns source node
TEST_F(ConditionalTransitionTest, OperatorReturnsSameSourceNode) {
    ConditionalTransition transition(sourceNode, testAction);

    auto result1 = transition >> targetNode;
    auto result2 = transition >> targetNode;

    EXPECT_EQ(result1, result2);
    EXPECT_EQ(result1, sourceNode);
}

// Test with nullptr target (edge case)
TEST_F(ConditionalTransitionTest, OperatorWithNullTarget) {
    ConditionalTransition transition(sourceNode, testAction);

    std::shared_ptr<BaseNode> nullTarget = nullptr;
    // This should not crash, but behavior depends on BaseNode::next implementation
    EXPECT_NO_THROW({
        auto result = transition >> nullTarget;
    });
}

// Test chaining multiple transitions
TEST_F(ConditionalTransitionTest, ChainingTransitions) {
    auto node1 = std::make_shared<MockNode>("node1");
    auto node2 = std::make_shared<MockNode>("node2");
    auto node3 = std::make_shared<MockNode>("node3");

    ConditionalTransition trans1(node1, "action1");
    auto result1 = trans1 >> node2;

    ConditionalTransition trans2(result1, "action2");
    auto result2 = trans2 >> node3;

    EXPECT_EQ(result1, node1);
    EXPECT_EQ(result2, result1);
}

// Test action name preservation
TEST_F(ConditionalTransitionTest, ActionNamePreservation) {
    std::string originalAction = "my_action";
    ConditionalTransition transition(sourceNode, originalAction);

    // Modify original string
    originalAction = "modified_action";

    // Transition should preserve its own copy
    EXPECT_EQ(transition.action, "my_action");
}

// Test with shared_ptr reference counting
TEST_F(ConditionalTransitionTest, SharedPtrReferenceCount) {
    auto node = std::make_shared<MockNode>("test");
    EXPECT_EQ(node.use_count(), 1);

    {
        ConditionalTransition transition(node, "action");
        EXPECT_EQ(node.use_count(), 2); // node + transition.src
    }

    EXPECT_EQ(node.use_count(), 1); // transition destroyed
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}