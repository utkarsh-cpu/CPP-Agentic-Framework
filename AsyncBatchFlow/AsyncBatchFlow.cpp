//
// Created by Utkarsh Kaushik on 18/05/25.
//

#include "AsyncBatchFlow.h"
#include "../BatchNode/VectorData.h"
#include "../BatchFlow/MapData.h"

std::future<std::shared_ptr<FrameworkData>> AsyncBatchFlow::runAsync(std::shared_ptr<FrameworkData> shared) {
    return std::async(std::launch::async, [this,shared]()-> std::shared_ptr<FrameworkData> {
        std::lock_guard<std::mutex> lock(shared_mutex);
        auto PrepRes=PrepAsync(shared).get();
        if (auto vecData = std::dynamic_pointer_cast<VectorData>(PrepRes)) {
            std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
            futures.reserve(vecData->value.size());
            for (const auto &bp:vecData->value) {
                auto params = this->params;
                if (auto mapData = std::dynamic_pointer_cast<MapData>(bp)) {
                    params.insert(mapData->value.begin(), mapData->value.end());
                }
                orchAsync(shared, params).get();
            }
        }
        std::shared_ptr<FrameworkData> ExecRes = nullptr;
        return PostAsync(shared, PrepRes,ExecRes ).get();
    });

}

std::shared_ptr<FrameworkData> AsyncBatchFlow::run(std::shared_ptr<FrameworkData> shared) {
    throw std::runtime_error("Use run_async instead of run for AsyncBatchFlow.");
}

std::shared_ptr<FrameworkData> AsyncBatchFlow::Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes) {
    return Flow::Post(shared, PrepRes, ExecRes);
}