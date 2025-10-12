//
// Created by Utkarsh Kaushik on 18/05/25.
//

#ifndef ASYNCPARALLELBATCHNODE_H
#define ASYNCPARALLELBATCHNODE_H
#include "../AsyncNode/AsyncNode.h"


class AsyncParallelBatchNode: public AsyncNode{
private:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override;


};

#endif //ASYNCPARALLELBATCHNODE_H
