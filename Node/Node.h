//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef NODE_H
#define NODE_H

#include "../BaseNode/BaseNode.h"

class Node : public BaseNode{
protected:
    int max_retries;
    int wait;
    int cur_retry;
public:
    explicit Node(int max_retries=1,int wait=0);
    virtual std::any exec_fallback(const std::any& PrepRes,const::exception & exc);
private:
    std::any exec(any &PrepRes) override;


};



#endif //NODE_H
