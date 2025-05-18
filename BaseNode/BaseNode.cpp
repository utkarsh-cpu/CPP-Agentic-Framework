#include "BaseNode.h"

#include <iostream>
using namespace std;


void BaseNode::setParam(map<string,any> Param) {
    params = std::move(Param);
}

shared_ptr<BaseNode> BaseNode::next(shared_ptr<BaseNode> Node,const string& Action) {
    if (successors.find(Action)!= successors.end()) {
        std::cerr<<"Warning:Overwritting successor for action '" << Action<<"'"<<std::endl;
    }
    successors[Action] = Node;
    return Node;
}

any BaseNode::Prep(any &shared) {return {};}
any BaseNode::Exec(any &PrepRes) {return {};}

any BaseNode::Post(any &shared, any &PrepRes, any &ExecRes) {return {};}

any BaseNode::exec(any &PrepRes) {
    return Exec(PrepRes);
}

any BaseNode::run(any &shared) {
    auto PrepRes=Prep(shared);
    auto ExecRes=exec(PrepRes);
    return Post(shared,PrepRes,ExecRes);
}

any BaseNode::Run(any &shared) {
    if(!successors.empty()){
        cerr<< "Warning: Node won't run successors. Use Flow.\n";
    }
    return run(shared);
}

shared_ptr<BaseNode> BaseNode::operator>>(const std::shared_ptr<BaseNode> &other) {
    return next(other);
}

ConditionalTransition BaseNode::operator-(const std::string &action) {
    return {this,action};

}

map<string,shared_ptr<BaseNode>> BaseNode::getSuccessor() {
    return successors;
}
