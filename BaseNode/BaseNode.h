#ifndef AGENTIC_FRAMEWORK_LIBRARY_H
#define AGENTIC_FRAMEWORK_LIBRARY_H



#include <any>

#include <memory>



#include<map>



class ConditionalTransition;





class BaseNode: public std::enable_shared_from_this<BaseNode> {



protected:

    std::map<std::string,std::any> params;

    std::map<std::string, std::shared_ptr<BaseNode>> successors;

public:

    BaseNode() =default;



    virtual ~BaseNode()= default;



    // Virtual methods for inheritance customization

    virtual void setParam(const std::map<std::string,std::any>& Param);

    virtual std::shared_ptr<BaseNode> next(std::shared_ptr<BaseNode> Node,const std::string& Action="default");

    virtual std::any Prep(std::any& shared);

    virtual std::any Exec(std::any& PrepRes);

    virtual std::any Post(std::any& shared,std::any& PrepRes,std::any& ExecRes);

    virtual std::any Run(std::any& shared);
    virtual std::any Execute(std::any& shared);



    // Operator overloads - kept non-virtual for specific behavior

    std::shared_ptr<BaseNode> operator>>(const std::shared_ptr<BaseNode> &other);

    ConditionalTransition operator-(const std::string& action);

    virtual std::map<std::string,std::shared_ptr<BaseNode>> getSuccessor();



protected:

    // Protected virtual methods for derived class customization

    virtual std::any exec(std::any &PrepRes);

    virtual std::any run(std::any& shared);



};



#endif //AGENTIC_FRAMEWORK_LIBRARY_H