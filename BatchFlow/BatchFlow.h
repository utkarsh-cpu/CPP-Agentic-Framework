//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef BATCHFLOW_H
#define BATCHFLOW_H

#include "../Flow/Flow.h"

class BatchFlow: public Flow{
    public:
    using Flow::Flow;
    private:
    std::any run(std::any& shared) override;
};



#endif //BATCHFLOW_H
