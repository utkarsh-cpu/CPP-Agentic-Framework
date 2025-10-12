//
// Created by Utkarsh Kaushik on 09/05/25.
//

#ifndef CONDITIONALTRANSITION_H
#define CONDITIONALTRANSITION_H

#include <memory>
#include <string>

class BaseNode;


class ConditionalTransition {

public:
    std::shared_ptr<BaseNode> src;
    std::string action;
    ConditionalTransition(std::shared_ptr<BaseNode> s,std::string   a);
    std::shared_ptr<BaseNode> operator>>(const std::shared_ptr<BaseNode>& tgt) const;

};




#endif //CONDITIONALTRANSITION_H
