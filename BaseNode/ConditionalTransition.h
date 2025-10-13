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
    
    ConditionalTransition(std::shared_ptr<BaseNode> s, std::string a);
    
    // Copy constructor and assignment
    ConditionalTransition(const ConditionalTransition& other) = default;
    ConditionalTransition& operator=(const ConditionalTransition& other) = default;
    
    // Move constructor and assignment
    ConditionalTransition(ConditionalTransition&& other) noexcept = default;
    ConditionalTransition& operator=(ConditionalTransition&& other) noexcept = default;
    
    [[nodiscard]] std::shared_ptr<BaseNode> operator>>(const std::shared_ptr<BaseNode>& tgt) const;
};




#endif //CONDITIONALTRANSITION_H
