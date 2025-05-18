//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef BATCHNODE_H
#define BATCHNODE_H

#include "../Node/Node.h"
#include<vector>

class BatchNode:public Node{
public:
    using Node::Node;
private:
    std::any exec(any &items) override;
};



#endif //BATCHNODE_H
