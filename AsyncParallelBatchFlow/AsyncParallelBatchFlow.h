//
// Created by Utkarsh Kaushik on 19/05/25.
//

#ifndef ASYNCPARALLELBATCHFLOW_H
#define ASYNCPARALLELBATCHFLOW_H
#include "../AsyncFlow/AsyncFlow.h"
#include "../BatchFlow/BatchFlow.h"


class AsyncParallelBatchFlow: virtual public AsyncFlow, virtual public BatchFlow{
public:
    using AsyncFlow::AsyncFlow;
    using BatchFlow::BatchFlow;
private:
    std::future<std::any> runAsync(std::any &shared) override;
    std::any run(std::any& shared) override;

};



#endif //ASYNCPARALLELBATCHFLOW_H
