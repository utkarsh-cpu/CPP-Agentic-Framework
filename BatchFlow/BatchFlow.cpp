//
// Created by Utkarsh Kaushik on 09/05/25.
//

#include "BatchFlow.h"

std::any BatchFlow::run(std::any &shared) {
    auto PrepRes = Prep(shared);
    std::vector<std::any> batch_params;
    if (PrepRes.has_value()) {
         batch_params = std::any_cast<std::vector<std::any> >(PrepRes);
    }
    for (const auto &bp:batch_params) {
        std::map<string, std::any> params=this->params;
        auto extras=std::any_cast<std::map<std::string, std::any>>(bp);
        params.insert(extras.begin(), extras.end());
        orch(shared,&params);
    }
    std::any ExecRes = nullptr;
    return Post(shared, PrepRes,ExecRes );
}
