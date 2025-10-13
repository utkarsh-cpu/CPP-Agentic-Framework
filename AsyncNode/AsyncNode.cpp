//
// Created by Utkarsh Kaushik on 10/05/25.
//

#include "AsyncNode.h"

#include <iostream>
#include <utility>
#include <thread>
#include <chrono>
#include <memory>


std::future<std::shared_ptr<FrameworkData>> AsyncNode::PrepAsync(std::shared_ptr<FrameworkData> shared) {
    (void)shared;
    return std::async(std::launch::async, []() -> std::shared_ptr<FrameworkData> { return nullptr; });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::ExecAsync(std::shared_ptr<FrameworkData> PrepRes) {
    (void)PrepRes;
    return std::async(std::launch::async, []() -> std::shared_ptr<FrameworkData> { return nullptr; });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::PostAsync(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes) {
    (void)shared; (void)PrepRes; (void)ExecRes;
    return std::async(std::launch::async, []() -> std::shared_ptr<FrameworkData> { return nullptr; });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::ExecFallbackAsync(std::shared_ptr<FrameworkData> PrepRes, const std::exception &exc) {
    (void)PrepRes;
    return std::async(std::launch::async, [&exc]() -> std::shared_ptr<FrameworkData> { throw exc; });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::runAsync(std::shared_ptr<FrameworkData> shared) {
    return std::async(std::launch::async, [this, shared]() -> std::shared_ptr<FrameworkData> {
        // RAII execution guard to prevent concurrent runs
        ExecutionGuard guard(is_executing_);
        
        try {
            auto prep_result = PrepAsync(shared).get();
            auto exec_result = exec(prep_result);
            return PostAsync(shared, prep_result, exec_result).get();
        } catch (...) {
            // Ensure cleanup happens on any exception
            cleanup_resources();
            throw;
        }
    });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::RunAsync(std::shared_ptr<FrameworkData> shared) {
    // Check for successors and warn if present
    if (!getSuccessor().empty()) {
        std::cerr << "Warning: Node will not run successors, Use AsyncFlow instead" << std::endl;
    }
    
    return std::async(std::launch::async, [this, shared = std::move(shared)]() -> std::shared_ptr<FrameworkData> {
        // RAII execution guard to prevent concurrent runs
        ExecutionGuard guard(is_executing_);
        
        try {
            auto prep_result = PrepAsync(shared).get();
            auto exec_result = exec(prep_result);
            return PostAsync(shared, prep_result, exec_result).get();
        } catch (...) {
            // Ensure cleanup happens on any exception
            cleanup_resources();
            throw;
        }
    });
}

std::shared_ptr<FrameworkData> AsyncNode::exec(std::shared_ptr<FrameworkData> PrepRes) {
    return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
        for (int i = 0; i < max_retries; i++) {
            try {
                auto futureResult = ExecAsync(PrepRes);
                if (std::shared_ptr<FrameworkData> result = futureResult.get()) {
                    return result;
                }
            }
            catch (const std::exception& e) {
                // Ensure cleanup happens on exception
                cleanup_resources();
                
                if (i == max_retries - 1) {
                    try {
                        auto fallbackResult = ExecFallbackAsync(PrepRes, e);
                        return fallbackResult.get();
                    } catch (...) {
                        // Log error and return nullptr rather than propagating
                        std::cerr << "AsyncNode exec: All retries exhausted and fallback failed" << std::endl;
                        return nullptr;
                    }
                }
                if (wait > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait));
                }
            }
        }
        return nullptr;
    }).get();
}

std::shared_ptr<FrameworkData> AsyncNode::run(std::shared_ptr<FrameworkData> shared) {
    (void)shared;
    throw std::runtime_error("Use RunAsync instead of run for AsyncNode.");
}

std::shared_ptr<FrameworkData> AsyncNode::Execute(std::shared_ptr<FrameworkData> shared) {
    return RunAsync(shared).get();
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::RunAsyncWithTimeout(
    std::shared_ptr<FrameworkData> shared, 
    std::chrono::milliseconds timeout) {
    
    return std::async(std::launch::async, [this, shared = std::move(shared), timeout]() -> std::shared_ptr<FrameworkData> {
        try {
            return execute_with_timeout([this](std::shared_ptr<FrameworkData> data) -> std::shared_ptr<FrameworkData> {
                return this->RunAsync(data).get();
            }, timeout, shared);
        } catch (const TimeoutException& e) {
            cleanup_resources();
            throw;
        } catch (...) {
            cleanup_resources();
            throw;
        }
    });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::RunAsyncWithCancellation(
    std::shared_ptr<FrameworkData> shared, 
    const CancellationToken& token) {
    
    return std::async(std::launch::async, [this, shared = std::move(shared), token]() -> std::shared_ptr<FrameworkData> {
        // Check for successors and warn if present
        if (!getSuccessor().empty()) {
            std::cerr << "Warning: Node will not run successors, Use AsyncFlow instead" << std::endl;
        }
        
        // RAII execution guard to prevent concurrent runs
        ExecutionGuard guard(is_executing_);
        
        try {
            // Check for cancellation before starting
            token.throw_if_cancellation_requested();
            
            auto prep_result = PrepAsyncWithCancellation(shared, token).get();
            
            // Check for cancellation after prep
            token.throw_if_cancellation_requested();
            
            auto exec_result = exec(prep_result);
            
            // Check for cancellation after exec
            token.throw_if_cancellation_requested();
            
            return PostAsyncWithCancellation(shared, prep_result, exec_result, token).get();
        } catch (const std::runtime_error& e) {
            // Check if this is a cancellation
            if (std::string(e.what()).find("cancelled") != std::string::npos) {
                cleanup_resources();
                throw OperationCancelledException(e.what());
            }
            cleanup_resources();
            throw;
        } catch (...) {
            cleanup_resources();
            throw;
        }
    });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::RunAsyncWithPolicy(
    std::shared_ptr<FrameworkData> shared, 
    const TimeoutPolicy& policy) {
    
    if (policy.cancellation_enabled()) {
        // Create a cancellation token with timeout
        auto token_source = std::make_shared<CancellationTokenSource>();
        
        // Schedule cancellation after timeout
        std::thread timeout_thread([token_source, timeout = policy.timeout()]() {
            std::this_thread::sleep_for(timeout);
            token_source->cancel();
        });
        timeout_thread.detach();
        
        return RunAsyncWithCancellation(shared, token_source->token());
    } else {
        return RunAsyncWithTimeout(shared, policy.timeout());
    }
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::RunAsyncWithPolicy(
    std::shared_ptr<FrameworkData> shared, 
    const std::string& policy_name) {
    
    auto policy = TimeoutPolicyRegistry::instance().get_policy(policy_name);
    return RunAsyncWithPolicy(shared, policy);
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::PrepAsyncWithCancellation(
    std::shared_ptr<FrameworkData> shared, const CancellationToken& token) {
    
    return std::async(std::launch::async, [this, shared, &token]() -> std::shared_ptr<FrameworkData> {
        token.throw_if_cancellation_requested();
        return PrepAsync(shared).get();
    });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::ExecAsyncWithCancellation(
    std::shared_ptr<FrameworkData> PrepRes, const CancellationToken& token) {
    
    return std::async(std::launch::async, [this, PrepRes, &token]() -> std::shared_ptr<FrameworkData> {
        token.throw_if_cancellation_requested();
        return ExecAsync(PrepRes).get();
    });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::PostAsyncWithCancellation(
    std::shared_ptr<FrameworkData> shared, 
    std::shared_ptr<FrameworkData> PrepRes, 
    std::shared_ptr<FrameworkData> ExecRes, 
    const CancellationToken& token) {
    
    return std::async(std::launch::async, [this, shared, PrepRes, ExecRes, &token]() -> std::shared_ptr<FrameworkData> {
        token.throw_if_cancellation_requested();
        return PostAsync(shared, PrepRes, ExecRes).get();
    });
}

void AsyncNode::cleanup_resources() noexcept {
    try {
        // Base implementation - derived classes can override for specific cleanup
        // This is where we would clean up any resources allocated during async operations
        // For now, this serves as a hook for derived classes to implement specific cleanup
        
        // Example cleanup operations that derived classes might implement:
        // - Close file handles
        // - Release network connections
        // - Clear temporary buffers
        // - Reset internal state
        
        // Since this is noexcept, we must not let any exceptions escape
    } catch (...) {
        // Log the error but don't propagate - this is a cleanup function
        std::cerr << "AsyncNode cleanup_resources: Exception during cleanup (suppressed)" << std::endl;
    }
}
