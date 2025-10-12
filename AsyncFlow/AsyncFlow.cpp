//
// Created by Utkarsh Kaushik on 18/05/25.
//

#include "AsyncFlow.h"

std::future<std::shared_ptr<FrameworkData>> AsyncFlow::orchAsync(std::shared_ptr<FrameworkData> shared, const std::map<std::string, std::shared_ptr<FrameworkData>> &Params) {
    return std::async(std::launch::async, [this,Params,shared]()-> std::shared_ptr<FrameworkData> {
        std::lock_guard<std::mutex> lock(shared_mutex);
        auto curr = startNode;
        const auto& p = Params.empty() ? this->params : Params;
        std::shared_ptr<FrameworkData> lastAction;
        while (curr) {
            curr->setParam(p);
            lastAction = curr->Execute(shared);
            if (auto stringData = std::dynamic_pointer_cast<StringData>(lastAction)) {
                curr=GetNextNode(curr, stringData->value);
            } else {
                curr = nullptr;
            }
        }
        return lastAction;
    });
}

std::future<std::shared_ptr<FrameworkData>> AsyncFlow::runAsync(std::shared_ptr<FrameworkData> shared) {
    return std::async(std::launch::async, [this,shared]()-> std::shared_ptr<FrameworkData> {
        std::lock_guard<std::mutex> lock(shared_mutex);
        auto Prep=PrepAsync(shared).get();
        auto Exec=orchAsync(shared).get();
        return PostAsync(shared,Prep,Exec).get();
    });
}


std::future<std::shared_ptr<FrameworkData>> AsyncFlow::PostAsync(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes) {
    return std::async(std::launch::async, [this,ExecRes]()-> std::shared_ptr<FrameworkData> {
        return ExecRes;
    });
}

std::shared_ptr<FrameworkData> AsyncFlow::run(std::shared_ptr<FrameworkData> shared) {
    throw std::runtime_error("Use run_async instead of run for AsyncFlow.");
}