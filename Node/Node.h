//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef NODE_H
#define NODE_H

#include "../BaseNode/BaseNode.h"

class Node : virtual public BaseNode{
protected:
    int cur_retry;

public:
    int max_retries;
    int wait;

    explicit Node(int max_retries=1,int wait=0);
    virtual ~Node() = default;

    // Virtual method for inheritance customization
    virtual std::shared_ptr<FrameworkData> exec_fallback(const std::shared_ptr<FrameworkData>& PrepRes, std::exception_ptr eptr);

protected:
    // Protected virtual method for derived class override
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> PrepRes) override;

};

#endif //NODE_H
