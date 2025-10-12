//
// Created by Utkarsh Kaushik on 09/05/25.
//

#include "ConditionalTransition.h"

#include <utility>

#include "BaseNode.h"

ConditionalTransition::ConditionalTransition(std::shared_ptr<BaseNode> s, std::string  a):src(std::move(s)),action(std::move(a)) {}

std::shared_ptr<BaseNode> ConditionalTransition::operator>>(const std::shared_ptr<BaseNode>& tgt) const {
    return src->next(tgt,action);
}
