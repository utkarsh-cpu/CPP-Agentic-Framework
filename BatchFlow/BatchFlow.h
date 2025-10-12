//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef BATCHFLOW_H
#define BATCHFLOW_H

#include "../Flow/Flow.h"

class BatchFlow:  virtual public Flow{
    public:
    using Flow::Flow;
    private:
    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override;
    std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes) override;
};

#endif //BATCHFLOW_H
