#include "gtest/gtest.h"
#include "../Flow/Flow.h"
#include "../Node/Node.h"
#include "../Flow/StringData.h"

class TestNode : public Node {
public:
    std::string return_value;
    TestNode(std::string val) : return_value(val) {}

    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> data) override {
        auto result = std::make_shared<StringData>();
        result->value = return_value;
        return result;
    }
    std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes) override {
        return ExecRes;
    }
};

TEST(FlowTest, BasicFlow) {
    auto node1 = std::make_shared<TestNode>("default");
    auto node2 = std::make_shared<TestNode>("default");
    auto node3 = std::make_shared<TestNode>("final_result");

    node1->next(node2);
    node2->next(node3);

    Flow flow(node1);
    auto result = flow.Run(nullptr);

    auto stringData = std::dynamic_pointer_cast<StringData>(result);
    ASSERT_TRUE(stringData != nullptr);
    ASSERT_EQ(stringData->value, "final_result");
}
