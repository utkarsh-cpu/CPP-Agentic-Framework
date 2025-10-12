#include "BaseNode.h"
#include "ConditionalTransition.h"
#include <iostream>


void BaseNode::setParam(const std::map<std::string,std::any>& Param) {
    params = Param;
}

std::shared_ptr<BaseNode> BaseNode::next(std::shared_ptr<BaseNode> Node,const std::string& Action) {
    if (successors.find(Action)!= successors.end()) {
        std::cerr<<"Warning:Overwritting successor for action '" << Action<<"'"<<std::endl;
    }
    successors[Action] = Node;
    return Node;
}

std::any BaseNode::Prep(std::any &shared) {return {};}
std::any BaseNode::Exec(std::any &PrepRes) {return {};}

std::any BaseNode::Post(std::any &shared, std::any &PrepRes, std::any &ExecRes) {return {};}

std::any BaseNode::exec(std::any &PrepRes) {
    return Exec(PrepRes);
}

std::any BaseNode::run(std::any &shared) {
    auto PrepRes=Prep(shared);
    auto ExecRes=exec(PrepRes);
    return Post(shared,PrepRes,ExecRes);
}

std::any BaseNode::Run(std::any &shared) {
    if(!successors.empty()){
        std::cerr<< "Warning: Node won't run successors. Use Flow.\n";
    }
    return run(shared);
}

std::any BaseNode::Execute(std::any &shared) {
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
