//
// Created by Utkarsh Kaushik on 18/05/25.
//

#ifndef ASYNCBATCHFLOW_H
#define ASYNCBATCHFLOW_H
#include "../AsyncFlow/AsyncFlow.h"


class AsyncBatchFlow :public AsyncFlow {
public:
    using AsyncFlow::AsyncFlow;
    std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes) override;
private:
    std::future<std::shared_ptr<FrameworkData>> runAsync(std::shared_ptr<FrameworkData> shared) override;
    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override;

};

#endif //ASYNCBATCHFLOW_H
