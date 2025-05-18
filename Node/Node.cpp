//
// Created by Utkarsh Kaushik on 09/05/25.
//

#include "Node.h"

#include <thread>

Node::Node(int max_retries, int wait) :max_retries(max_retries), wait(wait), cur_retry(0) {}

std::any Node::exec_fallback(const std::any& PrepRes,const::exception & exc) {
    throw exc;
}

std::any Node::exec(any &PrepRes) {
    for (cur_retry = 0; cur_retry < max_retries; ++cur_retry) {
        try {
            return Exec(PrepRes);
        } catch (const std::exception& e) {
            if (cur_retry == max_retries - 1) {
                return exec_fallback(PrepRes, e);
            }
            if (wait>0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait));
            }
        }
    }
    return {};
}




