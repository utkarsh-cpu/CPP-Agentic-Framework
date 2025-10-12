//
// Created by Utkarsh Kaushik on 13/05/25.
//

#include "AsyncBatchNode.h"
#include "../BatchNode/VectorData.h"

std::shared_ptr<FrameworkData> AsyncBatchNode::exec(std::shared_ptr<FrameworkData> items) {
    auto results = std::make_shared<VectorData>();
    if (auto vecData = std::dynamic_pointer_cast<VectorData>(items)) {
        std::vector<std::future<std::shared_ptr<FrameworkData>>> futures;
        futures.reserve(vecData->value.size());
        for (auto &item : vecData->value) {
            futures.push_back(AsyncNode::ExecAsync(item));
        }
        results->value.reserve(futures.size());
        for (auto &future : futures) {
            results->value.push_back(future.get());
        }
    }
    return results;
}