//
// Created by Utkarsh Kaushik on 09/05/25.
//

#include "BatchNode.h"
#include<vector>

std::shared_ptr<FrameworkData> BatchNode::exec(std::shared_ptr<FrameworkData> items) {
    auto results = std::make_shared<VectorData>();
    if (auto vecData = std::dynamic_pointer_cast<VectorData>(items)) {
        results->value.reserve(vecData->value.size());
        for (auto &item : vecData->value) {
            results->value.push_back(this->Exec(item));
        }
    }
    return results;
}