//
// Created by Utkarsh Kaushik on 13/05/25.
//

#ifndef ASYNCBATCHNODE_H
#define ASYNCBATCHNODE_H
#include "../AsyncNode/AsyncNode.h"


class AsyncBatchNode:public AsyncNode {
private:
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override;

};

#endif //ASYNCBATCHNODE_H
