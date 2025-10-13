//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef NODE_H
#define NODE_H

#include "../BaseNode/BaseNode.h"

class Node : virtual public BaseNode{
protected:
    std::atomic<int> cur_retry;

public:
    std::atomic<int> max_retries;
    std::atomic<int> wait;

    explicit Node(int max_retries=1,int wait=0);
    virtual ~Node() = default;

    // Virtual method for inheritance customization
    virtual std::shared_ptr<FrameworkData> exec_fallback(const std::shared_ptr<FrameworkData>& PrepRes, std::exception_ptr eptr) const;

protected:
    // Protected virtual method for derived class override
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> PrepRes) override;

};

#endif //NODE_H
