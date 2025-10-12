//
// Created by Utkarsh Kaushik on 13/05/25.
//

#include "AsyncBatchNode.h"

std::any AsyncBatchNode::exec(any &items) {
    std::vector<std::any> results;
    if (items.has_value() && items.type() == typeid(std::vector<std::any>)) {
        auto vec = std::any_cast<std::vector<std::any>>(items);
        std::vector<std::future<std::any>> futures;
        futures.reserve(vec.size());
        for (any &item : vec) {
            futures.push_back(AsyncNode::ExecAsync(item));
        }
        results.reserve(futures.size());
        for (auto &future : futures) {
            results.push_back(future.get());
        }
    }
    return results;
}