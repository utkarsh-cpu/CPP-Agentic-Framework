//
// Created by Utkarsh Kaushik on 09/05/25.
//

#include "BatchNode.h"
#include<vector>

std::any BatchNode::exec(any& items) {
    std::vector<std::any> results;
    if (!items.has_value() || items.type() == typeid(std::any)) {
        return {};
    }
    auto vec = std::any_cast<std::vector<std::any>>(items);
    results.reserve(vec.size());
    for (std::any &item : vec) {
        results.push_back(Node::Exec(item));
    }
    return results;
}