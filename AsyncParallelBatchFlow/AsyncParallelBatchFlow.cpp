//
// Created by Utkarsh Kaushik on 19/05/25.
//

#include "AsyncParallelBatchFlow.h"
#include "../BatchNode/VectorData.h"
#include "../BatchFlow/MapData.h"

std::future<std::shared_ptr<FrameworkData>> AsyncParallelBatchFlow::runAsync(std::shared_ptr<FrameworkData> shared) {
    return std::async(std::launch::async, [this, shared]() -> std::shared_ptr<FrameworkData> {
        std::lock_guard<std::mutex> lock(shared_mutex);
        auto PrepRes = PrepAsync(shared).get();
        
        if (auto vecData = std::dynamic_pointer_cast<VectorData>(PrepRes)) {
            std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
            futures.reserve(vecData->value.size());
            
            for (auto& bp : vecData->value) {
                auto params = this->getParams();
                if (auto mapData = std::dynamic_pointer_cast<MapData>(bp)) {
                    params.insert(mapData->value.begin(), mapData->value.end());
                }
                futures.push_back(orchAsync(shared, params));
            }
            
            for (auto& future : futures) {
                future.get();
            }
        }
        
        std::shared_ptr<FrameworkData> ExecRes = nullptr;
        return PostAsync(shared, PrepRes, ExecRes).get();
    });
}

std::shared_ptr<FrameworkData> AsyncParallelBatchFlow::run(std::shared_ptr<FrameworkData> shared) {
    (void)shared;
    throw std::runtime_error("Use runAsync instead of run for AsyncParallelBatchFlow.");
}

std::shared_ptr<FrameworkData> AsyncParallelBatchFlow::Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes) {
    return Flow::Post(shared, PrepRes, ExecRes);
}
