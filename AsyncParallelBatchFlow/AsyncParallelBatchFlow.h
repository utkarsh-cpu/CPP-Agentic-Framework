//
// Created by Utkarsh Kaushik on 19/05/25.
//

#ifndef ASYNCPARALLELBATCHFLOW_H
#define ASYNCPARALLELBATCHFLOW_H
#include "../AsyncFlow/AsyncFlow.h"


class AsyncParallelBatchFlow: virtual public AsyncFlow {
public:
    using AsyncFlow::AsyncFlow;
    std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes) override;
private:
    std::future<std::shared_ptr<FrameworkData>> runAsync(std::shared_ptr<FrameworkData> shared) override;
    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override;

};

#endif //ASYNCPARALLELBATCHFLOW_H
