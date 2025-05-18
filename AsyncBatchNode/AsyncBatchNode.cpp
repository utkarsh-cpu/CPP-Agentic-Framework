//
// Created by Utkarsh Kaushik on 13/05/25.
//

#include "AsyncBatchNode.h"

std::any AsyncBatchNode::exec(any &items) {
    std::vector<std::any> results;
    if (items.has_value()) {
        for (auto vec = std::any_cast<std::vector<std::any>>(items); any &item : vec) {
            results.push_back(AsyncNode::ExecAsync(item).get());
        }
    }
    return results;
}