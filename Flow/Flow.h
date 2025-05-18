//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef FLOW_H
#define FLOW_H

#include "../BaseNode/BaseNode.h"

class Flow: public BaseNode {
protected:
    std::shared_ptr<BaseNode> startNode;
public:
    explicit Flow(std::shared_ptr<BaseNode> StartNode=nullptr);
    std::shared_ptr<BaseNode> Start(std::shared_ptr<BaseNode> start);
    static std::shared_ptr<BaseNode> GetNextNode(const std::shared_ptr<BaseNode>& curr,const string &action);
    std::any Post(std::any& shared,std::any& PrepRes,std::any& ExecRes) override;
    virtual std::any orch(std::any& shared,std::map<std::string,std::any>*Params=nullptr);
private:
    std::any run(std::any& shared) override;


};



#endif //FLOW_H
