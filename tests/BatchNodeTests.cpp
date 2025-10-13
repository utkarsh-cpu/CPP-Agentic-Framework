#include "gtest/gtest.h"
#include "../BatchNode/BatchNode.h"
#include "../BatchNode/VectorData.h"
#include "../Flow/StringData.h"

class TestBatchNode : public BatchNode {
public:
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> data) override {
        if (auto stringData = std::dynamic_pointer_cast<StringData>(data)) {
            auto result = std::make_shared<StringData>();
            result->value = "success";
            return result;
        }
        return nullptr;
    }
    std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes) override {
        (void)shared; (void)PrepRes; // Suppress unused parameter warnings
        return ExecRes;
    }
};

TEST(BatchNodeTest, BasicTest) {
    TestBatchNode node;
    auto data = std::make_shared<VectorData>();
    data->value.push_back(std::make_shared<StringData>());
    data->value.push_back(std::make_shared<StringData>());

    auto result = node.Run(data);
    auto vecData = std::dynamic_pointer_cast<VectorData>(result);
    ASSERT_TRUE(vecData != nullptr);
    ASSERT_EQ(vecData->value.size(), 2);
    for (auto& item : vecData->value) {
        auto stringData = std::dynamic_pointer_cast<StringData>(item);
        ASSERT_TRUE(stringData != nullptr);
        ASSERT_EQ(stringData->value, "success");
    }
}
