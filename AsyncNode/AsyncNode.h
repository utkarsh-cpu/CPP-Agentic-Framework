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
    virtual std::future<std::shared_ptr<FrameworkData>> PrepAsync(std::shared_ptr<FrameworkData> shared);
    virtual std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes);
    virtual std::future<std::shared_ptr<FrameworkData>> ExecFallbackAsync(std::shared_ptr<FrameworkData> PrepRes,const std::exception& exc);
    virtual std::future<std::shared_ptr<FrameworkData>> PostAsync(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes);
    virtual std::future<std::shared_ptr<FrameworkData>> RunAsync(std::shared_ptr<FrameworkData> shared);
    std::shared_ptr<FrameworkData> Execute(std::shared_ptr<FrameworkData> shared) override;

protected:
    // Protected virtual methods for derived class customization
    virtual std::future<std::shared_ptr<FrameworkData>> runAsync(std::shared_ptr<FrameworkData> shared);
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> PrepRes) override;
    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override;

};

#endif //ASYNCNODE_H
