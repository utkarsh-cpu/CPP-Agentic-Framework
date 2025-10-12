//
// Created by Utkarsh Kaushik on 19/05/25.
//

#include "AsyncParallelBatchFlow.h"

std::future<std::any> AsyncParallelBatchFlow::runAsync(std::any &shared) {
    return std::async(std::launch::async, [this, &shared]() -> std::any {
        std::lock_guard<std::mutex> lock(shared_mutex);
        std::any Prep = PrepAsync(shared).get();
        
        // Handle empty or invalid prep result
        if (!Prep.has_value() || Prep.type() != typeid(std::vector<std::any>)) {
            std::any exec{};
            return PostAsync(shared, Prep, exec).get();
        }
        
        auto batch = std::any_cast<std::vector<std::any>>(Prep);
        std::vector<std::future<std::any>> futures;
        futures.reserve(batch.size());
        
        // Launch all batch items in parallel (equivalent to asyncio.gather)
        for (auto& bp : batch) {
            std::map<std::string, std::any> Params = this->params;
            if (bp.has_value() && bp.type() == typeid(std::map<std::string, std::any>)) {
                auto extras = std::any_cast<std::map<std::string, std::any>>(bp);
                Params.insert(extras.begin(), extras.end());
            }
            futures.push_back(orchAsync(shared, &Params));
        }
        
        // Wait for all parallel operations to complete
        for (auto& future : futures) {
            future.get(); // This blocks until the future is ready
        }
        
        std::any ExecRes = nullptr;
        return PostAsync(shared, Prep, ExecRes).get();
    });
}
std::any AsyncParallelBatchFlow::run(std::any& shared) {
    throw std::runtime_error("Use runAsync instead of run for AsyncParallelBatchFlow.");
}

