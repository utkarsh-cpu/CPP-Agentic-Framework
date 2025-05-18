//
// Created by Utkarsh Kaushik on 13/05/25.
//

#ifndef ASYNCBATCHNODE_H
#define ASYNCBATCHNODE_H
#include "../AsyncNode/AsyncNode.h"
#include "../BatchNode/BatchNode.h"


class AsyncBatchNode:public AsyncNode,public BatchNode {
private:
    std::any exec(any &items) override;

};



#endif //ASYNCBATCHNODE_H
