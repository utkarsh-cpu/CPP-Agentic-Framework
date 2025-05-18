//
// Created by Utkarsh Kaushik on 09/05/25.
//

#include "Flow.h"

#include <iostream>
#include <ostream>
Flow::Flow(std::shared_ptr<BaseNode> StartNode): startNode(std::move(StartNode)){}

std::shared_ptr<BaseNode> Flow::Start(std::shared_ptr<BaseNode> start) {
    startNode = start;
    return start;
}

std::shared_ptr<BaseNode> Flow::GetNextNode(const std::shared_ptr<BaseNode>& curr, const string &action) {
    const auto it=curr->getSuccessor().find(action.empty()?"default":action);
    if (it==curr->getSuccessor().end() && !curr->getSuccessor().empty()) {
        std::cerr<< "Flow ends: '" << action << "'not found in the successors" << std::endl;
        return nullptr;
    }
    return it!=curr->getSuccessor().end()?it->second:nullptr;
}

std::any Flow::orch(std::any &shared, std::map<std::string, std::any> *Params) {
    auto curr = startNode;
    const auto p= Params? *Params : this->params;
    std::string lastAction;
    while (curr) {
        curr->setParam(p);
        lastAction = std::any_cast<std::string>(curr->Run(shared));
        curr=GetNextNode(curr,lastAction);
    }
    return lastAction;
}

std::any Flow::run(std::any &shared) {
    auto PrepRes= Prep(shared);
    auto ExecRes= orch(shared);
    return Post(shared,PrepRes,ExecRes);
}

std::any Flow::Post(std::any &shared,std::any &PrepRes,std::any &ExecRes) {
    return ExecRes;
}