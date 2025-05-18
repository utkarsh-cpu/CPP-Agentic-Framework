//
// Created by Utkarsh Kaushik on 18/05/25.
//

#ifndef ASYNCFLOW_H
#define ASYNCFLOW_H
#include "../AsyncNode/AsyncNode.h"
#include "../Flow/Flow.h"


class AsyncFlow: public Flow,public AsyncNode{
public:
    using Flow::Flow;
    using AsyncNode::AsyncNode;
    std::future<std::any> PostAsync(std::any& shared,std::any& PrepRes,std::any& ExecRes) override;
private:
    std::future<std::any> orchAsync(std::any& shared,const std::map<std::string,std::any>*Params=nullptr);
    std::future<std::any> runAsync(std::any &shared) override;

};



#endif //ASYNCFLOW_H
