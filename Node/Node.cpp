//
// Created by Utkarsh Kaushik on 09/05/25.
//

#include "Node.h"

#include <thread>

Node::Node(int max_retries, int wait) :max_retries(max_retries), wait(wait), cur_retry(0) {}

std::shared_ptr<FrameworkData> Node::exec_fallback(const std::shared_ptr<FrameworkData>& PrepRes, std::exception_ptr eptr) {
    std::rethrow_exception(eptr);
}

std::shared_ptr<FrameworkData> Node::exec(std::shared_ptr<FrameworkData> PrepRes) {
    for (cur_retry = 0; cur_retry < max_retries; ++cur_retry) {
        try {
            return Exec(PrepRes);
        } catch (...) {
            if (cur_retry == max_retries - 1) {
                return exec_fallback(PrepRes, std::current_exception());
            }
            if (wait>0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait));
            }
        }
    }
    return nullptr;
}
