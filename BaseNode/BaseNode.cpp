#include "BaseNode.h"
#include "ConditionalTransition.h"
#include "InputValidation.h"
#include <iostream>
#include <shared_mutex>
#include <mutex>

void BaseNode::setParam(const std::map<std::string, std::shared_ptr<FrameworkData>>& Param) {
    // Input validation
    Security::InputValidator::validate_container_size(Param, 1000, "parameter map");
    
    // Validate each parameter
    for (const auto& [key, value] : Param) {
        Security::InputValidator::validate_non_empty_string(key, "parameter key");
        Security::InputValidator::validate_string_length(key, 256, "parameter key");
        Security::BoundsChecker::check_not_null(value, "parameter value for key '" + key + "'");
    }
    
    std::unique_lock<std::shared_mutex> lock(params_mutex_);
    params_ = Param;
}

void BaseNode::setParam(std::map<std::string, std::shared_ptr<FrameworkData>>&& Param) {
    // Input validation
    Security::InputValidator::validate_container_size(Param, 1000, "parameter map");
    
    // Validate each parameter
    for (const auto& [key, value] : Param) {
        Security::InputValidator::validate_non_empty_string(key, "parameter key");
        Security::InputValidator::validate_string_length(key, 256, "parameter key");
        Security::BoundsChecker::check_not_null(value, "parameter value for key '" + key + "'");
    }
    
    std::unique_lock<std::shared_mutex> lock(params_mutex_);
    params_ = std::move(Param);
}

std::shared_ptr<FrameworkData> BaseNode::getParam(std::string_view key) const {
    // Input validation
    std::string key_str{key}; // Convert to string for validation
    Security::InputValidator::validate_non_empty_string(key_str, "parameter key");
    Security::InputValidator::validate_string_length(key_str, 256, "parameter key");
    
    std::shared_lock<std::shared_mutex> lock(params_mutex_);
    auto it = params_.find(key_str);
    return it != params_.end() ? it->second : nullptr;
}

std::map<std::string, std::shared_ptr<FrameworkData>> BaseNode::getParams() const {
    std::shared_lock<std::shared_mutex> lock(params_mutex_);
    return params_;
}

std::shared_ptr<BaseNode> BaseNode::next(std::shared_ptr<BaseNode> Node, std::string_view Action) {
    // Input validation
    Security::BoundsChecker::check_not_null(Node, "successor node");
    std::string action_str{Action}; // Convert to string for validation
    Security::InputValidator::validate_string_length(action_str, 256, "action name");
    
    std::unique_lock<std::shared_mutex> lock(successors_mutex_);
    
    // Check for maximum number of successors to prevent resource exhaustion
    Security::InputValidator::validate_container_size(successors_, 100, "successors map");
    
    auto [it, inserted] = successors_.try_emplace(action_str, Node);
    if (!inserted) {
        std::cerr << "Warning: Overwriting successor for action '" << action_str << "'" << std::endl;
        it->second = Node;
    }
    return it->second;
}


std::shared_ptr<FrameworkData> BaseNode::Prep(std::shared_ptr<FrameworkData> shared) {(void)shared; return nullptr;}
std::shared_ptr<FrameworkData> BaseNode::Exec(std::shared_ptr<FrameworkData> PrepRes) {(void)PrepRes; return nullptr;}

std::shared_ptr<FrameworkData> BaseNode::Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes) {(void)shared; (void)PrepRes; (void)ExecRes; return nullptr;}

std::shared_ptr<FrameworkData> BaseNode::exec(std::shared_ptr<FrameworkData> PrepRes) {
    return Exec(PrepRes);
}

std::shared_ptr<FrameworkData> BaseNode::run(std::shared_ptr<FrameworkData> shared) {
    auto PrepRes=Prep(shared);
    auto ExecRes=exec(PrepRes);
    return Post(shared,PrepRes,ExecRes);
}

std::shared_ptr<FrameworkData> BaseNode::Run(std::shared_ptr<FrameworkData> shared) {
    {
        std::shared_lock<std::shared_mutex> lock(successors_mutex_);
        if(!successors_.empty()){
            std::cerr<< "Warning: Node won't run successors. Use Flow.\n";
        }
    }
    return run(shared);
}

std::shared_ptr<FrameworkData> BaseNode::Execute(std::shared_ptr<FrameworkData> shared) {
    return Run(shared);
}

std::shared_ptr<BaseNode> BaseNode::operator>>(const std::shared_ptr<BaseNode> &other) {
    return next(other);
}

ConditionalTransition BaseNode::operator-(std::string_view action) {
    return {shared_from_this(), std::string{action}};
}

std::map<std::string,std::shared_ptr<BaseNode>> BaseNode::getSuccessor() const {
    std::shared_lock<std::shared_mutex> lock(successors_mutex_);
    return successors_;
}

// Safe parameter access methods implementation
std::shared_ptr<FrameworkData> BaseNode::getParamSafe(std::string_view key) const {
    auto param = getParam(key);
    if (!param) {
        throw Security::InputValidationException("Required parameter '" + std::string{key} + "' not found");
    }
    return param;
}

void BaseNode::setParamSafe(std::string_view key, std::shared_ptr<FrameworkData> value) {
    std::string key_str{key}; // Convert to string for validation
    Security::InputValidator::validate_non_empty_string(key_str, "parameter key");
    Security::InputValidator::validate_string_length(key_str, 256, "parameter key");
    Security::BoundsChecker::check_not_null(value, "parameter value");
    
    std::unique_lock<std::shared_mutex> lock(params_mutex_);
    
    // Check for maximum number of parameters to prevent resource exhaustion
    Security::InputValidator::validate_container_size(params_, 1000, "parameters map");
    
    params_[key_str] = std::move(value);
}

void BaseNode::validateParams() const {
    std::shared_lock<std::shared_mutex> lock(params_mutex_);
    
    for (const auto& [key, value] : params_) {
        if (key.empty()) {
            throw Security::InputValidationException("Empty parameter key found");
        }
        if (key.length() > 256) {
            throw Security::InputValidationException("Parameter key '" + key + "' exceeds maximum length");
        }
        if (!value) {
            throw Security::InputValidationException("Null parameter value for key '" + key + "'");
        }
    }
}
