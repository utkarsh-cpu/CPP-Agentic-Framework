#include "BaseNode.h"
#include "ConditionalTransition.h"
#include <iostream>

void BaseNode::setParam(const std::map<std::string, std::shared_ptr<FrameworkData>>& Param) {
    params = Param;
}

std::shared_ptr<BaseNode> BaseNode::next(std::shared_ptr<BaseNode> Node,const std::string& Action) {
    if (successors.find(Action)!= successors.end()) {
        std::cerr<<"Warning:Overwritting successor for action '" << Action<<"'"<<std::endl;
    }
    successors[Action] = Node;
    return Node;
}

std::shared_ptr<FrameworkData> BaseNode::Prep(std::shared_ptr<FrameworkData> shared) {return nullptr;}
std::shared_ptr<FrameworkData> BaseNode::Exec(std::shared_ptr<FrameworkData> PrepRes) {return nullptr;}

std::shared_ptr<FrameworkData> BaseNode::Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes) {return nullptr;}

std::shared_ptr<FrameworkData> BaseNode::exec(std::shared_ptr<FrameworkData> PrepRes) {
    return Exec(PrepRes);
}

std::shared_ptr<FrameworkData> BaseNode::run(std::shared_ptr<FrameworkData> shared) {
    auto PrepRes=Prep(shared);
    auto ExecRes=exec(PrepRes);
    return Post(shared,PrepRes,ExecRes);
}

std::shared_ptr<FrameworkData> BaseNode::Run(std::shared_ptr<FrameworkData> shared) {
    if(!successors.empty()){
        std::cerr<< "Warning: Node won't run successors. Use Flow.\n";
    }
    return run(shared);
}

std::shared_ptr<FrameworkData> BaseNode::Execute(std::shared_ptr<FrameworkData> shared) {
    return Run(shared);
}

std::shared_ptr<BaseNode> BaseNode::operator>>(const std::shared_ptr<BaseNode> &other) {
    return next(other);
}

ConditionalTransition BaseNode::operator-(const std::string &action) {
    return {shared_from_this(),action};

}

std::map<std::string,std::shared_ptr<BaseNode>> BaseNode::getSuccessor() {
    return successors;
}
