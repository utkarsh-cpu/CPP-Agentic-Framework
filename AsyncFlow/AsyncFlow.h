//
// Created by Utkarsh Kaushik on 18/05/25.
//

#ifndef ASYNCFLOW_H
#define ASYNCFLOW_H
#include "../AsyncNode/AsyncNode.h"
#include "../Flow/Flow.h"
#include <mutex>


class AsyncFlow: virtual public Flow, virtual public AsyncNode{
protected:
    std::future<std::shared_ptr<FrameworkData>> orchAsync(std::shared_ptr<FrameworkData> shared,const std::map<std::string,std::shared_ptr<FrameworkData>>& Params = {});
    std::mutex shared_mutex;
public:
    using Flow::Flow;
    using AsyncNode::AsyncNode;
    std::future<std::shared_ptr<FrameworkData>> PostAsync(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes) override;
private:
    std::future<std::shared_ptr<FrameworkData>> runAsync(std::shared_ptr<FrameworkData> shared) override;
    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override;

};

#endif //ASYNCFLOW_H
