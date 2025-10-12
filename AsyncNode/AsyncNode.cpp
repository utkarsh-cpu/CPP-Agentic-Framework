//
// Created by Utkarsh Kaushik on 10/05/25.
//

#include "AsyncNode.h"

#include <iostream>


std::future<std::shared_ptr<FrameworkData>> AsyncNode::PrepAsync(std::shared_ptr<FrameworkData> shared) {
    return std::async(std::launch::async, []() -> std::shared_ptr<FrameworkData> { return nullptr; });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::ExecAsync(std::shared_ptr<FrameworkData> PrepRes) {
    return std::async(std::launch::async, []() -> std::shared_ptr<FrameworkData> { return nullptr; });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::PostAsync(std::shared_ptr<FrameworkData> shared,std::shared_ptr<FrameworkData> PrepRes,std::shared_ptr<FrameworkData> ExecRes) {
    return std::async(std::launch::async, []() -> std::shared_ptr<FrameworkData> { return nullptr; });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::ExecFallbackAsync(std::shared_ptr<FrameworkData> PrepRes, const std::exception &exc) {
    return std::async(std::launch::async, [&exc]() -> std::shared_ptr<FrameworkData> { throw exc; });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::runAsync(std::shared_ptr<FrameworkData> shared) {
    return std::async(std::launch::async, [this,shared]()-> std::shared_ptr<FrameworkData> {
        auto p = PrepAsync(shared).get();
        auto e=exec(p);
        return PostAsync(shared,p,e).get();
    });
}

std::future<std::shared_ptr<FrameworkData>> AsyncNode::RunAsync(std::shared_ptr<FrameworkData> shared) {
    if (!successors.empty()) {
        std::cerr << "Warning: Node will not run successors, Use AsyncFlow instead"<< std::endl;
    }
    return runAsync(shared);
}

std::shared_ptr<FrameworkData> AsyncNode::exec(std::shared_ptr<FrameworkData> PrepRes) {
    return std::async(std::launch::async, [this, PrepRes]() -> std::shared_ptr<FrameworkData> {
        for (int i = 0; i < max_retries; i++) {
            try {
                auto futureResult = ExecAsync(PrepRes);
                auto result = futureResult.get();
                if (result) {
                    return result;
                }
            }
            catch (const std::exception& e) {
                if (i == max_retries - 1) {
                    auto fallbackResult = ExecFallbackAsync(PrepRes, e);
                    return fallbackResult.get();
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
    throw std::runtime_error("Use run_async instead of run for AsyncNode.");
}

std::shared_ptr<FrameworkData> AsyncNode::Execute(std::shared_ptr<FrameworkData> shared) {
    return RunAsync(shared).get();
}
