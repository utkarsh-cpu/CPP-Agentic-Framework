//
// Created by Utkarsh Kaushik on 18/05/25.
//

#include "AsyncParallelBatchNode.h"

std::any AsyncParallelBatchNode::exec(std::any &items) {
   std::vector<std::any> results;
   std::vector<std::future<std::any>> futures;
   if (items.has_value()) {
      for (auto itemsVec = std::any_cast<std::vector<std::any>>(items); auto &item : itemsVec) {
         futures.push_back(AsyncNode::ExecAsync(item));
      }
   }
   results.reserve(futures.size());
   for (auto &future : futures) {
      results.push_back(future.get());
   }
   return results;
}