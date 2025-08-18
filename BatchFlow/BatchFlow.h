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
    std::any run(std::any& shared) override;
    std::any Post(std::any& shared,std::any& PrepRes,std::any& ExecRes) override;
};



#endif //BATCHFLOW_H
