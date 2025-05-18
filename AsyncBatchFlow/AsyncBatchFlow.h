//
// Created by Utkarsh Kaushik on 18/05/25.
//

#ifndef ASYNCBATCHFLOW_H
#define ASYNCBATCHFLOW_H
#include "../AsyncFlow/AsyncFlow.h"
#include "../BatchFlow/BatchFlow.h"


class AsyncBatchFlow :public AsyncFlow,public BatchFlow{
public:
    using AsyncFlow::AsyncFlow;
    using BatchFlow::BatchFlow;
private:
    std::future<std::any> runAsync(std::any &shared) override;;

};



#endif //ASYNCBATCHFLOW_H
