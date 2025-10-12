//
// Created by Utkarsh Kaushik on 09/05/25.
//

#include "BatchFlow.h"
#include "../BatchNode/VectorData.h"
#include "MapData.h"

std::shared_ptr<FrameworkData> BatchFlow::run(std::shared_ptr<FrameworkData> shared) {
    auto PrepRes = Prep(shared);
    if (auto vecData = std::dynamic_pointer_cast<VectorData>(PrepRes)) {
        for (const auto &bp:vecData->value) {
            auto params = this->params;
            if (auto mapData = std::dynamic_pointer_cast<MapData>(bp)) {
                params.insert(mapData->value.begin(), mapData->value.end());
            }
            orch(shared,params);
        }
    }
    std::shared_ptr<FrameworkData> ExecRes = nullptr;
    return Post(shared, PrepRes,ExecRes );
}

std::shared_ptr<FrameworkData> BatchFlow::Post(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes) {
    return Flow::Post(shared,PrepRes,ExecRes);
}