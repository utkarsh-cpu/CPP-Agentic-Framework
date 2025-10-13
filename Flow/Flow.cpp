#include "Flow.h"
#include "../BaseNode/InputValidation.h"

#include <iostream>
#include <ostream>
Flow::Flow(std::shared_ptr<BaseNode> StartNode): startNode(std::move(StartNode)){}

std::shared_ptr<BaseNode> Flow::Start(std::shared_ptr<BaseNode> start) {
    Security::BoundsChecker::check_not_null(start, "start node");
    startNode = start;
    return start;
}

std::shared_ptr<BaseNode> Flow::GetNextNode(const std::shared_ptr<BaseNode>& curr, std::string_view action) {
    Security::BoundsChecker::check_not_null(curr, "current node");
    std::string action_str{action};
    Security::InputValidator::validate_string_length(action_str, 256, "action name");
    
    const auto& successors = curr->getSuccessor();
    const std::string lookup_key = action_str.empty() ? "default" : action_str;
    const auto it = successors.find(lookup_key);
    
    if (it == successors.end() && !successors.empty()) {
        std::cerr << "Flow ends: '" << action_str << "' not found in the successors" << std::endl;
        return nullptr;
    }
    return it != successors.end() ? it->second : nullptr;
}

std::shared_ptr<FrameworkData> Flow::orch(std::shared_ptr<FrameworkData> shared, const std::map<std::string, std::shared_ptr<FrameworkData>> &Params) {
    auto curr = startNode;
    const auto& p = Params.empty() ? this->getParams() : Params;
    std::string lastAction;
    while (curr) {
        try {
            curr->setParam(p);

            auto runResult = curr->Run(shared);

            if (auto stringData = std::dynamic_pointer_cast<StringData>(runResult)) {
                lastAction = stringData->value;
            } else {
                std::cerr << "Error: Run() did not return a StringData object." << std::endl;
                auto errorData = std::make_shared<StringData>();
                errorData->value = "error";
                return errorData;
            }

            if (!curr) {
                std::cerr << "Current node became null during execution" << std::endl;
                break;
            }

            curr = GetNextNode(curr, lastAction);

        } catch (const std::exception& e) {
            std::cerr << "Exception in Flow::orch: " << e.what() << std::endl;
            auto errorData = std::make_shared<StringData>();
            errorData->value = "error";
            return errorData;
        } catch (...) {
            std::cerr << "Unknown exception in Flow::orch" << std::endl;
            auto errorData = std::make_shared<StringData>();
            errorData->value = "error";
            return errorData;
        }
    }
    auto resultData = std::make_shared<StringData>();
    resultData->value = lastAction;
    return resultData;
}

std::shared_ptr<FrameworkData> Flow::run(std::shared_ptr<FrameworkData> shared) {
    auto PrepRes= Prep(shared);
    auto ExecRes= orch(shared);
    return Post(shared,PrepRes,ExecRes);
}

std::shared_ptr<FrameworkData> Flow::Post(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes) {
    (void)shared; (void)PrepRes;
    return ExecRes;
}

std::shared_ptr<FrameworkData> Flow::orch(std::shared_ptr<FrameworkData> shared, std::map<std::string, std::shared_ptr<FrameworkData>>&& Params) {
    auto curr = startNode;
    const auto& p = Params.empty() ? this->getParams() : Params;
    std::string lastAction;
    while (curr) {
        try {
            curr->setParam(p);

            auto runResult = curr->Run(shared);

            if (auto stringData = std::dynamic_pointer_cast<StringData>(runResult)) {
                lastAction = std::move(stringData->value);
            } else {
                std::cerr << "Error: Run() did not return a StringData object." << std::endl;
                auto errorData = std::make_shared<StringData>();
                errorData->value = "error";
                return errorData;
            }

            if (!curr) {
                std::cerr << "Current node became null during execution" << std::endl;
                break;
            }

            curr = GetNextNode(curr, lastAction);

        } catch (const std::exception& e) {
            std::cerr << "Exception in Flow::orch: " << e.what() << std::endl;
            auto errorData = std::make_shared<StringData>();
            errorData->value = "error";
            return errorData;
        } catch (...) {
            std::cerr << "Unknown exception in Flow::orch" << std::endl;
            auto errorData = std::make_shared<StringData>();
            errorData->value = "error";
            return errorData;
        }
    }
    auto resultData = std::make_shared<StringData>();
    resultData->value = std::move(lastAction);
    return resultData;
}