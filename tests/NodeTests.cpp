#include "gtest/gtest.h"
#include "../Node/Node.h"
#include "../Flow/StringData.h"

// A simple node that returns a string
class TestNode : public Node {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> data) override {
        auto result = std::make_shared<StringData>();
        result->value = "success";
        return result;
    }
    std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes) override {
        return ExecRes;
    }
};

TEST(NodeTest, RunTest) {
    TestNode node;
    auto result = node.Run(nullptr);
    auto stringData = std::dynamic_pointer_cast<StringData>(result);
    ASSERT_TRUE(stringData != nullptr);
    ASSERT_EQ(stringData->value, "success");
}

// A node that fails a specified number of times
class FailingNode : public Node {
public:
    int fail_count;
    int current_count = 0;

    FailingNode(int fail_count) : fail_count(fail_count) {}

    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> data) override {
        if (current_count < fail_count) {
            current_count++;
            throw std::runtime_error("Failed");
        }
        auto result = std::make_shared<StringData>();
        result->value = "success";
        return result;
    }
    std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes) override {
        return ExecRes;
    }
};

TEST(NodeTest, RetryTest) {
    FailingNode node(2); // Fails twice, then succeeds
    Node& base_node = node;
    base_node.max_retries = 3;

    auto result = base_node.Run(nullptr);
    auto stringData = std::dynamic_pointer_cast<StringData>(result);
    ASSERT_TRUE(stringData != nullptr);
    ASSERT_EQ(stringData->value, "success");
}

TEST(NodeTest, RetryFailTest) {
    FailingNode node(3); // Fails three times
    Node& base_node = node;
    base_node.max_retries = 3;

    ASSERT_THROW(base_node.Run(nullptr), std::runtime_error);
}