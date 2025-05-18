//
// Created by Utkarsh Kaushik on 10/05/25.
//

#include "AsyncNode.h"

#include <iostream>


std::future<std::any> AsyncNode::PrepAsync(std::any &shared) {
    return std::async(std::launch::async, []() -> std::any { return {}; });
}

std::future<std::any> AsyncNode::ExecAsync(std::any &PrepRes) {
    return std::async(std::launch::async, []() -> std::any { return {}; });
}

std::future<std::any> AsyncNode::PostAsync(std::any& shared,std::any &PrepRes,std::any& ExecRes) {
    return std::async(std::launch::async, []() -> std::any { return {}; });
}

std::future<std::any> AsyncNode::ExecFallbackAsync(std::any &PrepRes, const exception &exc) {
    return std::async(std::launch::async, [&exc]() -> std::any { throw exc; });
}

std::future<std::any> AsyncNode::runAsync(std::any &shared) {
    return std::async(std::launch::async, [this,shared]()-> std::any {
        auto p = PrepAsync(shared).get();
        auto e=exec(p);
        return PostAsync(shared,p,e).get();
    });
}

std::future<std::any> AsyncNode::RunAsync(std::any &shared) {
    if (!successors.empty()) {
        std::cerr << "Warning: Node will not run successors, Use AsyncFlow instead"<< std::endl;
    }
    return runAsync(shared);
}

std::any AsyncNode::exec(std::any &PrepRes) {
    return std::async(std::launch::async, [this, PrepRes]() -> std::any {
        for (int i = 0; i < max_retries; i++) {
            try {
                auto futureResult = ExecAsync(PrepRes);
                return futureResult.get();
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
        return {};
    }).get();
}

std::any AsyncNode::run(std::any &shared) {
    throw std::runtime_error("Use run_async instead of run for AsyncNode.");
}
