#ifndef AGENTIC_FRAMEWORK_LIBRARY_H
#define AGENTIC_FRAMEWORK_LIBRARY_H

#include <memory>
#include <map>
#include <string>
#include "FrameworkData.h"

#include "ConditionalTransition.h"

class BaseNode: public std::enable_shared_from_this<BaseNode> {

protected:
    std::map<std::string, std::shared_ptr<FrameworkData>> params;
    std::map<std::string, std::shared_ptr<BaseNode>> successors;

public:
    BaseNode() =default;

    virtual ~BaseNode()= default;

    // Virtual methods for inheritance customization
    virtual void setParam(const std::map<std::string, std::shared_ptr<FrameworkData>>& Param);
    virtual std::shared_ptr<BaseNode> next(std::shared_ptr<BaseNode> Node,const std::string& Action="default");
    virtual std::shared_ptr<FrameworkData> Prep(std::shared_ptr<FrameworkData> shared);
    virtual std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> PrepRes);
    virtual std::shared_ptr<FrameworkData> Post(std::shared_ptr<FrameworkData> shared, std::shared_ptr<FrameworkData> PrepRes, std::shared_ptr<FrameworkData> ExecRes);
    virtual std::shared_ptr<FrameworkData> Run(std::shared_ptr<FrameworkData> shared);
    virtual std::shared_ptr<FrameworkData> Execute(std::shared_ptr<FrameworkData> shared);

    // Operator overloads - kept non-virtual for specific behavior
    std::shared_ptr<BaseNode> operator>>(const std::shared_ptr<BaseNode> &other);
    ConditionalTransition operator-(const std::string& action);
    virtual std::map<std::string,std::shared_ptr<BaseNode>> getSuccessor();

protected:
    // Protected virtual methods for derived class customization
    virtual std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> PrepRes);
    virtual std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared);

};

#endif //AGENTIC_FRAMEWORK_LIBRARY_H