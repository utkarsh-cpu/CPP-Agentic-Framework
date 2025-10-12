//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef FLOW_H
#define FLOW_H

#include "../BaseNode/BaseNode.h"
#include "StringData.h"

class Flow: virtual public BaseNode {
protected:
    std::shared_ptr<BaseNode> startNode;

public:
    explicit Flow(std::shared_ptr<BaseNode> StartNode=nullptr);
    ~Flow() override = default;

    // Virtual methods for inheritance customization
    virtual std::shared_ptr<BaseNode> Start(std::shared_ptr<BaseNode> start);
    static std::shared_ptr<BaseNode> GetNextNode(const std::shared_ptr<BaseNode>& curr,const std::string &action);
    std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes) override;
    virtual std::shared_ptr<FrameworkData> orch(std::shared_ptr<FrameworkData> shared, const std::map<std::string, std::shared_ptr<FrameworkData>>& Params = {});

protected:
    // Protected virtual method for derived class customization
    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override;

};

#endif //FLOW_H
