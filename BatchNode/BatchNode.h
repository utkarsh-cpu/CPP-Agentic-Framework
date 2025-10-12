//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef BATCHNODE_H
#define BATCHNODE_H

#include "../Node/Node.h"
#include "VectorData.h"
#include<vector>

class BatchNode: virtual public Node{
public:
    using Node::Node;
    virtual ~BatchNode() = default;

protected:
    // Protected virtual method for derived class customization
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> items) override;
};

#endif //BATCHNODE_H
