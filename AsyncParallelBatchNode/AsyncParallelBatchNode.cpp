//
// Created by Utkarsh Kaushik on 18/05/25.
//

#include "AsyncParallelBatchNode.h"

std::any AsyncParallelBatchNode::exec(std::any &items) {
   std::vector<std::any> results;
   std::vector<std::future<std::any>> futures;
   for (auto item : items) {
      futures.push_back(AsyncNode::ExecAsync(item));
   }
   for (auto &future : futures) {
      results.push_back(future.get());
   }
   return results;
}