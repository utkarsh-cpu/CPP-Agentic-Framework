#ifndef AGENTIC_FRAMEWORK_LIBRARY_H
#define AGENTIC_FRAMEWORK_LIBRARY_H

#include <memory>
#include <map>
#include <string>
#include <shared_mutex>
#include <mutex>
#include "FrameworkData.h"
#include "InputValidation.h"

#include "ConditionalTransition.h"

class BaseNode: public std::enable_shared_from_this<BaseNode> {

private:
    // Thread-safe synchronization primitives
    mutable std::shared_mutex params_mutex_;
    mutable std::shared_mutex successors_mutex_;

protected:
    std::map<std::string, std::shared_ptr<FrameworkData>> params_;
    std::map<std::string, std::shared_ptr<BaseNode>> successors_;

public:
    BaseNode() =default;

    virtual ~BaseNode()= default;

    // Virtual methods for inheritance customization with input validation
    virtual void setParam(const std::map<std::string, std::shared_ptr<FrameworkData>>& Param);
    virtual void setParam(std::map<std::string, std::shared_ptr<FrameworkData>>&& Param);
    [[nodiscard]] virtual std::shared_ptr<FrameworkData> getParam(std::string_view key) const;
    [[nodiscard]] virtual std::map<std::string, std::shared_ptr<FrameworkData>> getParams() const;
    virtual std::shared_ptr<BaseNode> next(std::shared_ptr<BaseNode> Node, std::string_view Action = "default");
    
    // Safe parameter access methods
    [[nodiscard]] virtual std::shared_ptr<FrameworkData> getParamSafe(std::string_view key) const;
    virtual void setParamSafe(std::string_view key, std::shared_ptr<FrameworkData> value);
    virtual void validateParams() const;
    virtual std::shared_ptr<FrameworkData> Prep(std::shared_ptr<FrameworkData> shared);
    virtual std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> PrepRes);
    virtual std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes);
    virtual std::shared_ptr<FrameworkData> Run(std::shared_ptr<FrameworkData> shared);
    virtual std::shared_ptr<FrameworkData> Execute(std::shared_ptr<FrameworkData> shared);

    // Operator overloads - kept non-virtual for specific behavior
    std::shared_ptr<BaseNode> operator>>(const std::shared_ptr<BaseNode> &other);
    ConditionalTransition operator-(std::string_view action);
    [[nodiscard]] virtual std::map<std::string,std::shared_ptr<BaseNode>> getSuccessor() const;

protected:
    // Protected virtual methods for derived class customization
    virtual std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> PrepRes);
    virtual std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared);

};

#endif //AGENTIC_FRAMEWORK_LIBRARY_H