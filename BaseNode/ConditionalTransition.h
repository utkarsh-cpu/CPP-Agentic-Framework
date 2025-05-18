//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef CONDITIONALTRANSITION_H
#define CONDITIONALTRANSITION_H

#include "BaseNode.h"
class BaseNode;


class ConditionalTransition {

public:
    BaseNode* src;
    std::string action;
    ConditionalTransition(BaseNode* s,const std::string&  a);
    std::shared_ptr<BaseNode> operator>>(const std::shared_ptr<BaseNode>& tgt) const;

};




#endif //CONDITIONALTRANSITION_H
