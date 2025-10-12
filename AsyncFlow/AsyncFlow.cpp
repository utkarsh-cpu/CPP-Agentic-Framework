//
// Created by Utkarsh Kaushik on 18/05/25.
//

#include "AsyncFlow.h"

std::future<std::any> AsyncFlow::orchAsync(std::any &shared, const std::map<std::string, std::any> *Params) {
    return std::async(std::launch::async, [this,Params,&shared]()-> std::any {
        std::lock_guard<std::mutex> lock(shared_mutex);
        auto curr = startNode;
        const auto p= Params? *Params : Flow::params;
        std::any lastAction;
        while (curr) {
            curr->setParam(p);
            lastAction = curr->Execute(shared);
            curr=GetNextNode(curr,std::any_cast<std::string>(lastAction));
        }
        return lastAction;
    });
}

std::future<std::any> AsyncFlow::runAsync(std::any &shared) {
    return std::async(std::launch::async, [this,&shared]()-> std::any {
        std::lock_guard<std::mutex> lock(shared_mutex);
        std::any Prep=PrepAsync(shared).get();
        std::any Exec=orchAsync(shared).get();
        return PostAsync(shared,Prep,Exec).get();
    });
}


std::future<std::any> AsyncFlow::PostAsync(std::any& shared,std::any& PrepRes,std::any& ExecRes) {
    return std::async(std::launch::async, [this,ExecRes]()-> std::any {
        return ExecRes;
    });
}

std::any AsyncFlow::run(std::any& shared) {
    throw std::runtime_error("Use run_async instead of run for AsyncFlow.");
}