#ifndef AGENTIC_FRAMEWORK_LIBRARY_H
#define AGENTIC_FRAMEWORK_LIBRARY_H

#include <any>
#include <memory>

#include<map>

class ConditionalTransition;

using namespace std;

class BaseNode {

protected:
    map<string,any> params;
    map<string, shared_ptr<BaseNode>> successors;
public:
    BaseNode() =default;

    virtual ~BaseNode()= default;

    // Virtual methods for inheritance customization
    virtual void setParam(map<string,any> Param);
    virtual shared_ptr<BaseNode> next(shared_ptr<BaseNode> Node,const string& Action="default");
    virtual any Prep(any& shared);
    virtual any Exec(any& PrepRes);
    virtual any Post(any& shared,any& PrepRes,any& ExecRes);
    virtual any Run(any& shared);

    // Operator overloads - kept non-virtual for specific behavior
    shared_ptr<BaseNode> operator>>(const std::shared_ptr<BaseNode> &other);
    ConditionalTransition operator-(const std::string& action);
    virtual map<string,shared_ptr<BaseNode>> getSuccessor();

protected:
    // Protected virtual methods for derived class customization
    virtual any exec(any &PrepRes);
    virtual any run(any& shared);

};

#endif //AGENTIC_FRAMEWORK_LIBRARY_H