//
// Created by Utkarsh Kaushik on 18/05/25.
//

#ifndef ASYNCPARALLELBATCHNODE_H
#define ASYNCPARALLELBATCHNODE_H
#include "../AsyncNode/AsyncNode.h"
#include "../BatchNode/BatchNode.h"


class AsyncParallelBatchNode: public AsyncNode,public BatchNode{
private:
    std::any exec(std::any &items) override;


};



#endif //ASYNCPARALLELBATCHNODE_H
