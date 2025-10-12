//
// Created by Utkarsh Kaushik on 18/05/25.
//

#include "AsyncBatchFlow.h"

std::future<std::any> AsyncBatchFlow::runAsync(std::any &shared) {
    return std::async(std::launch::async, [this,&shared]()-> std::any {
        std::lock_guard<std::mutex> lock(shared_mutex);
        std::any Prep=PrepAsync(shared).get();
        if (!Prep.has_value() || Prep.type()!=typeid(std::vector<std::any>)) {
            std::any exec={};
            return PostAsync(shared,Prep,exec).get();
        }
        auto batch = std::any_cast<std::vector<std::any>>(Prep);
        std::vector<std::future<std::any>> futures;
        futures.reserve(batch.size());
        for (auto&bp: batch) {
            std::map<string, std::any> Params= this->params;
            if (bp.has_value() && bp.type() == typeid(std::map<std::string, std::any>)) {
                auto extras=std::any_cast<std::map<std::string, std::any>>(bp);
                Params.insert(extras.begin(), extras.end());
            }
            orchAsync(shared,&Params).get();
        }
        std::any ExecRes = nullptr;
        return PostAsync(shared, Prep,ExecRes ).get();
    });

};

std::any AsyncBatchFlow::run(std::any& shared) {
    throw std::runtime_error("Use run_async instead of run for AsyncBatchFlow.");
}