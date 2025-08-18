//
// Created by Utkarsh Kaushik on 10/05/25.
//

#ifndef ASYNCNODE_H
#define ASYNCNODE_H

#include "../Node/Node.h"
#include <future>

class AsyncNode: virtual public Node{
public:
    using Node::Node;
    virtual ~AsyncNode() = default;

    // Virtual async methods for inheritance customization
    virtual std::future<std::any> PrepAsync(std::any& shared);
    virtual std::future<std::any> ExecAsync(std::any& PrepRes);
    virtual std::future<std::any> ExecFallbackAsync(std::any& PrepRes,const std::exception& exc);
    virtual std::future<std::any> PostAsync(std::any& shared,std::any& PrepRes,std::any& ExecRes);
    virtual std::future<std::any> RunAsync(std::any& shared);

protected:
    // Protected virtual methods for derived class customization
    virtual std::future<std::any> runAsync(std::any& shared);
    std::any exec(std::any& PrepRes) override;
    std::any run(std::any& shared) override;

};



#endif //ASYNCNODE_H
