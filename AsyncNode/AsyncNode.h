//
// Created by Utkarsh Kaushik on 10/05/25.
//

#ifndef ASYNCNODE_H
#define ASYNCNODE_H

#include "../Node/Node.h"
#include "../BaseNode/AsyncTimeout.h"
#include "../BaseNode/CancellationToken.h"
#include "../BaseNode/TimeoutPolicy.h"
#include <future>
#include <atomic>
#include <mutex>
#include <stdexcept>

class AsyncNode: virtual public Node{
private:
    // Thread-safe execution tracking
    std::atomic<bool> is_executing_{false};
    mutable std::mutex execution_mutex_;

    // RAII execution guard for managing execution state
    class ExecutionGuard {
    private:
        std::atomic<bool>& flag_;
        bool acquired_;
        
    public:
        explicit ExecutionGuard(std::atomic<bool>& flag) : flag_(flag), acquired_(false) {
            bool expected = false;
            if (flag_.compare_exchange_strong(expected, true)) {
                acquired_ = true;
            } else {
                throw std::runtime_error("AsyncNode already executing - concurrent execution not allowed");
            }
        }
        
        ~ExecutionGuard() noexcept {
            if (acquired_) {
                flag_ = false;
            }
        }
        
        // Non-copyable and non-movable
        ExecutionGuard(const ExecutionGuard&) = delete;
        ExecutionGuard& operator=(const ExecutionGuard&) = delete;
        ExecutionGuard(ExecutionGuard&&) = delete;
        ExecutionGuard& operator=(ExecutionGuard&&) = delete;
    };

public:
    using Node::Node;
    virtual ~AsyncNode() = default;

    // Virtual async methods for inheritance customization
    virtual std::future<std::shared_ptr<FrameworkData>> PrepAsync(std::shared_ptr<FrameworkData> shared);
    virtual std::future<std::shared_ptr<FrameworkData>> ExecAsync(std::shared_ptr<FrameworkData> PrepRes);
    virtual std::future<std::shared_ptr<FrameworkData>> ExecFallbackAsync(std::shared_ptr<FrameworkData> PrepRes,const std::exception& exc);
    virtual std::future<std::shared_ptr<FrameworkData>> PostAsync(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes);
    
    // Modern async execution with proper error handling and resource management
    virtual std::future<std::shared_ptr<FrameworkData>> RunAsync(std::shared_ptr<FrameworkData> shared);
    
    // Async execution with timeout support
    virtual std::future<std::shared_ptr<FrameworkData>> RunAsyncWithTimeout(
        std::shared_ptr<FrameworkData> shared, 
        std::chrono::milliseconds timeout);
    
    // Async execution with cancellation support
    virtual std::future<std::shared_ptr<FrameworkData>> RunAsyncWithCancellation(
        std::shared_ptr<FrameworkData> shared, 
        const CancellationToken& token);
    
    // Async execution with timeout policy
    virtual std::future<std::shared_ptr<FrameworkData>> RunAsyncWithPolicy(
        std::shared_ptr<FrameworkData> shared, 
        const TimeoutPolicy& policy);
    
    // Async execution with named timeout policy
    virtual std::future<std::shared_ptr<FrameworkData>> RunAsyncWithPolicy(
        std::shared_ptr<FrameworkData> shared, 
        const std::string& policy_name);
    
    std::shared_ptr<FrameworkData> Execute(std::shared_ptr<FrameworkData> shared) override;

    // Check if node is currently executing
    [[nodiscard]] bool is_executing() const noexcept { return is_executing_.load(); }

protected:
    // Protected virtual methods for derived class customization
    virtual std::future<std::shared_ptr<FrameworkData>> runAsync(std::shared_ptr<FrameworkData> shared);
    
    // Protected methods with cancellation support for derived classes
    virtual std::future<std::shared_ptr<FrameworkData>> PrepAsyncWithCancellation(
        std::shared_ptr<FrameworkData> shared, const CancellationToken& token);
    virtual std::future<std::shared_ptr<FrameworkData>> ExecAsyncWithCancellation(
        std::shared_ptr<FrameworkData> PrepRes, const CancellationToken& token);
    virtual std::future<std::shared_ptr<FrameworkData>> PostAsyncWithCancellation(
        std::shared_ptr<FrameworkData> shared, 
        std::shared_ptr<FrameworkData> PrepRes, 
        std::shared_ptr<FrameworkData> ExecRes, 
        const CancellationToken& token);
    
    std::shared_ptr<FrameworkData> exec(std::shared_ptr<FrameworkData> PrepRes) override;
    std::shared_ptr<FrameworkData> run(std::shared_ptr<FrameworkData> shared) override;

private:
    // Resource cleanup for async operations
    virtual void cleanup_resources() noexcept;
};

#endif //ASYNCNODE_H
