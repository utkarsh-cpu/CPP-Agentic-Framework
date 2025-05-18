cmake_minimum_required(VERSION 3.30)
project(agentic_framework)

set(CMAKE_CXX_STANDARD 20)

add_library(agentic_framework STATIC BaseNode/BaseNode.cpp
        Node/Node.cpp
        Node/Node.h
        BaseNode/ConditionalTransition.cpp
        BaseNode/ConditionalTransition.h
        BatchNode/BatchNode.cpp
        BatchNode/BatchNode.h
        Flow/Flow.cpp
        Flow/Flow.h
        BatchFlow/BatchFlow.cpp
        BatchFlow/BatchFlow.h
        AsyncNode/AsyncNode.cpp
        AsyncNode/AsyncNode.h
        AsyncBatchNode/AsyncBatchNode.cpp
        AsyncBatchNode/AsyncBatchNode.h
        AsyncParallelBatchNode/AsyncParallelBatchNode.cpp
        AsyncParallelBatchNode/AsyncParallelBatchNode.h
        AsyncFlow/AsyncFlow.cpp
        AsyncFlow/AsyncFlow.h
        AsyncBatchFlow/AsyncBatchFlow.cpp
        AsyncBatchFlow/AsyncBatchFlow.h
)
