#include "gtest/gtest.h"
#include "../BaseNode/BaseNode.h"
#include "../Flow/StringData.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <future>
#include <barrier>

// Test data class for concurrency testing
class TestData : public FrameworkData {
public:
    std::string value;
    int counter = 0;
    
    TestData(const std::string& val = "") : value(val) {}
    
    [[nodiscard]] std::unique_ptr<FrameworkData> clone() const override {
        auto cloned = std::make_unique<TestData>(value);
        cloned->counter = counter;
        return cloned;
    }
    
    [[nodiscard]] const std::type_info& type() const noexcept override {
        return typeid(TestData);
    }
};

// Test node implementation for concurrency testing
class ConcurrentTestNode : public BaseNode {
public:
    std::atomic<int> exec_count{0};
    
    std::shared_ptr<FrameworkData> Exec(std::shared_ptr<FrameworkData> data) override {
        exec_count++;
        std::this_thread::sleep_for(std::chrono::microseconds(10)); // Simulate work
        return std::make_shared<TestData>("executed");
    }
};

class BaseNodeConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        node = std::make_shared<ConcurrentTestNode>();
    }
    
    std::shared_ptr<ConcurrentTestNode> node;
    static constexpr int NUM_THREADS = 10;
    static constexpr int OPERATIONS_PER_THREAD = 100;
};

// Test 1: Concurrent parameter access from multiple threads
TEST_F(BaseNodeConcurrencyTest, ConcurrentParameterAccess) {
    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    std::atomic<int> race_condition_detected{0};
    
    // Barrier to synchronize thread start
    std::barrier sync_point(NUM_THREADS);
    
    // Pre-populate with some parameters to test concurrent access
    std::map<std::string, std::shared_ptr<FrameworkData>> initial_params;
    for (int i = 0; i < NUM_THREADS; ++i) {
        initial_params["shared_param_" + std::to_string(i)] = 
            std::make_shared<TestData>("initial_value_" + std::to_string(i));
    }
    node->setParam(initial_params);
    
    // Launch threads that concurrently read parameters and occasionally update them
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([this, i, &successful_operations, &race_condition_detected, &sync_point]() {
            sync_point.arrive_and_wait(); // Synchronize start
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, NUM_THREADS - 1);
            
            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                try {
                    if (j % 10 == 0) {
                        // Occasionally update parameters (10% of operations)
                        std::map<std::string, std::shared_ptr<FrameworkData>> params;
                        for (int k = 0; k < NUM_THREADS; ++k) {
                            params["shared_param_" + std::to_string(k)] = 
                                std::make_shared<TestData>("updated_by_" + std::to_string(i) + "_" + std::to_string(j));
                        }
                        node->setParam(params);
                    } else {
                        // Mostly read operations (90% of operations)
                        std::string key = "shared_param_" + std::to_string(dis(gen));
                        auto retrieved = node->getParam(key);
                        
                        if (retrieved) {
                            auto test_data = std::dynamic_pointer_cast<TestData>(retrieved);
                            if (test_data) {
                                // Just verify we can read the data without crashes
                                volatile auto value_length = test_data->value.length();
                                (void)value_length; // Suppress unused variable warning
                                successful_operations++;
                            }
                        }
                    }
                    
                    // Small delay to increase contention
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    
                } catch (const std::exception& e) {
                    race_condition_detected++;
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify no race conditions caused crashes or exceptions
    EXPECT_EQ(race_condition_detected.load(), 0) << "Race conditions detected causing exceptions";
    EXPECT_GT(successful_operations.load(), 0) << "No successful operations recorded";
    
    // Verify final state is consistent
    auto final_params = node->getParams();
    EXPECT_EQ(final_params.size(), NUM_THREADS) << "Final parameter count is incorrect";
}

// Test 2: Concurrent parameter reading while writing
TEST_F(BaseNodeConcurrencyTest, ConcurrentParameterReadWrite) {
    const int NUM_READERS = 5;
    const int NUM_WRITERS = 3;
    const int DURATION_MS = 1000;
    
    std::atomic<bool> stop_flag{false};
    std::atomic<int> read_operations{0};
    std::atomic<int> write_operations{0};
    std::atomic<int> read_errors{0};
    
    // Initialize with some parameters
    std::map<std::string, std::shared_ptr<FrameworkData>> initial_params;
    for (int i = 0; i < 10; ++i) {
        initial_params["init_param_" + std::to_string(i)] = 
            std::make_shared<TestData>("initial_value_" + std::to_string(i));
    }
    node->setParam(initial_params);
    
    std::vector<std::thread> threads;
    
    // Reader threads
    for (int i = 0; i < NUM_READERS; ++i) {
        threads.emplace_back([this, i, &stop_flag, &read_operations, &read_errors]() {
            while (!stop_flag.load()) {
                try {
                    // Read all parameters
                    auto params = node->getParams();
                    
                    // Verify consistency - all initial params should be present
                    for (int j = 0; j < 10; ++j) {
                        std::string key = "init_param_" + std::to_string(j);
                        auto it = params.find(key);
                        if (it == params.end()) {
                            read_errors++;
                            break;
                        }
                    }
                    
                    read_operations++;
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                } catch (const std::exception& e) {
                    read_errors++;
                }
            }
        });
    }
    
    // Writer threads
    for (int i = 0; i < NUM_WRITERS; ++i) {
        threads.emplace_back([this, i, &stop_flag, &write_operations]() {
            int counter = 0;
            while (!stop_flag.load()) {
                try {
                    std::map<std::string, std::shared_ptr<FrameworkData>> params;
                    
                    // Keep initial params and add new ones
                    for (int j = 0; j < 10; ++j) {
                        params["init_param_" + std::to_string(j)] = 
                            std::make_shared<TestData>("initial_value_" + std::to_string(j));
                    }
                    
                    // Add writer-specific parameter
                    params["writer_" + std::to_string(i) + "_param"] = 
                        std::make_shared<TestData>("writer_value_" + std::to_string(counter++));
                    
                    node->setParam(params);
                    write_operations++;
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                } catch (const std::exception& e) {
                    // Writer errors are less critical for this test
                }
            }
        });
    }
    
    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    stop_flag = true;
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify operations completed successfully
    EXPECT_GT(read_operations.load(), 0) << "No read operations completed";
    EXPECT_GT(write_operations.load(), 0) << "No write operations completed";
    EXPECT_EQ(read_errors.load(), 0) << "Read operations encountered errors due to race conditions";
}

// Test 3: Concurrent successor management
TEST_F(BaseNodeConcurrencyTest, ConcurrentSuccessorManagement) {
    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    std::atomic<int> failed_operations{0};
    
    std::barrier sync_point(NUM_THREADS);
    
    // Create successor nodes
    std::vector<std::shared_ptr<ConcurrentTestNode>> successor_nodes;
    for (int i = 0; i < NUM_THREADS; ++i) {
        successor_nodes.push_back(std::make_shared<ConcurrentTestNode>());
    }
    
    // Launch threads that concurrently manage successors
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([this, i, &successful_operations, &failed_operations, 
                             &sync_point, &successor_nodes]() {
            sync_point.arrive_and_wait();
            
            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                try {
                    std::string action = "action_" + std::to_string(i) + "_" + std::to_string(j);
                    
                    // Add successor
                    auto successor = successor_nodes[i % successor_nodes.size()];
                    node->next(successor, action);
                    
                    // Small delay to increase contention
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    
                    // Verify successor was added
                    auto successors = node->getSuccessor();
                    auto it = successors.find(action);
                    
                    if (it != successors.end() && it->second == successor) {
                        successful_operations++;
                    } else {
                        failed_operations++;
                    }
                } catch (const std::exception& e) {
                    failed_operations++;
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify operations completed successfully
    EXPECT_EQ(failed_operations.load(), 0) << "Some successor operations failed due to race conditions";
    EXPECT_GT(successful_operations.load(), 0) << "No successful successor operations recorded";
    
    // Verify final state consistency
    auto final_successors = node->getSuccessor();
    EXPECT_EQ(final_successors.size(), NUM_THREADS * OPERATIONS_PER_THREAD) 
        << "Final successor count doesn't match expected";
}

// Test 4: High contention stress test
TEST_F(BaseNodeConcurrencyTest, HighContentionStressTest) {
    const int STRESS_THREADS = 20;
    const int STRESS_OPERATIONS = 500;
    const int SHARED_PARAMS = 5;
    
    std::vector<std::thread> threads;
    std::atomic<int> total_operations{0};
    std::atomic<int> failed_operations{0};
    
    std::barrier sync_point(STRESS_THREADS);
    
    // Launch high-contention threads
    for (int i = 0; i < STRESS_THREADS; ++i) {
        threads.emplace_back([this, i, &total_operations, &failed_operations, 
                             &sync_point, STRESS_OPERATIONS, SHARED_PARAMS]() {
            sync_point.arrive_and_wait();
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> param_dis(0, SHARED_PARAMS - 1);
            std::uniform_int_distribution<> op_dis(0, 2); // 0=set, 1=get, 2=successor
            
            for (int j = 0; j < STRESS_OPERATIONS; ++j) {
                try {
                    int operation = op_dis(gen);
                    
                    if (operation == 0) {
                        // Set parameter operation
                        std::string key = "shared_param_" + std::to_string(param_dis(gen));
                        std::map<std::string, std::shared_ptr<FrameworkData>> params;
                        params[key] = std::make_shared<TestData>("thread_" + std::to_string(i) + 
                                                                "_op_" + std::to_string(j));
                        node->setParam(params);
                        
                    } else if (operation == 1) {
                        // Get parameter operation
                        std::string key = "shared_param_" + std::to_string(param_dis(gen));
                        auto param = node->getParam(key);
                        // Just accessing is enough for the test
                        
                    } else {
                        // Successor operation
                        std::string action = "stress_action_" + std::to_string(i) + "_" + std::to_string(j);
                        auto successor = std::make_shared<ConcurrentTestNode>();
                        node->next(successor, action);
                    }
                    
                    total_operations++;
                    
                } catch (const std::exception& e) {
                    failed_operations++;
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify stress test results
    int expected_operations = STRESS_THREADS * STRESS_OPERATIONS;
    EXPECT_EQ(total_operations.load() + failed_operations.load(), expected_operations)
        << "Total operations don't match expected count";
    
    // Allow for some failures under extreme stress, but not too many
    double failure_rate = static_cast<double>(failed_operations.load()) / expected_operations;
    EXPECT_LT(failure_rate, 0.01) << "Failure rate too high: " << (failure_rate * 100) << "%";
}

// Test 5: Deadlock prevention test
TEST_F(BaseNodeConcurrencyTest, DeadlockPreventionTest) {
    const int DEADLOCK_THREADS = 8;
    const int DEADLOCK_OPERATIONS = 200;
    
    std::vector<std::thread> threads;
    std::atomic<bool> deadlock_detected{false};
    std::atomic<int> completed_operations{0};
    
    // Create multiple nodes to test cross-node operations
    std::vector<std::shared_ptr<ConcurrentTestNode>> nodes;
    for (int i = 0; i < 4; ++i) {
        nodes.push_back(std::make_shared<ConcurrentTestNode>());
    }
    
    std::barrier sync_point(DEADLOCK_THREADS);
    
    // Launch threads that perform operations across multiple nodes
    for (int i = 0; i < DEADLOCK_THREADS; ++i) {
        threads.emplace_back([this, i, &nodes, &deadlock_detected, &completed_operations,
                             &sync_point, DEADLOCK_OPERATIONS]() {
            sync_point.arrive_and_wait();
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> node_dis(0, nodes.size() - 1);
            
            auto start_time = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::seconds(5); // 5 second timeout to detect deadlocks
            
            for (int j = 0; j < DEADLOCK_OPERATIONS && !deadlock_detected.load(); ++j) {
                try {
                    auto current_time = std::chrono::steady_clock::now();
                    if (current_time - start_time > timeout) {
                        deadlock_detected = true;
                        break;
                    }
                    
                    // Perform operations on random nodes
                    auto node1 = nodes[node_dis(gen)];
                    auto node2 = nodes[node_dis(gen)];
                    
                    // Set parameters on first node
                    std::map<std::string, std::shared_ptr<FrameworkData>> params1;
                    params1["cross_param_" + std::to_string(i)] = 
                        std::make_shared<TestData>("cross_value_" + std::to_string(j));
                    node1->setParam(params1);
                    
                    // Set successor on second node pointing to first
                    std::string action = "cross_action_" + std::to_string(i) + "_" + std::to_string(j);
                    node2->next(node1, action);
                    
                    // Get parameters from both nodes
                    auto param1 = node1->getParam("cross_param_" + std::to_string(i));
                    auto successors2 = node2->getSuccessor();
                    
                    completed_operations++;
                    
                } catch (const std::exception& e) {
                    // Exceptions are acceptable, deadlocks are not
                }
            }
        });
    }
    
    // Wait for all threads to complete or timeout
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify no deadlock occurred
    EXPECT_FALSE(deadlock_detected.load()) << "Deadlock detected during concurrent operations";
    EXPECT_GT(completed_operations.load(), 0) << "No operations completed - possible deadlock";
}

// Test 6: Memory consistency and thread safety validation
TEST_F(BaseNodeConcurrencyTest, MemoryConsistencyTest) {
    const int CONSISTENCY_THREADS = 6;
    const int CONSISTENCY_OPERATIONS = 200;
    
    std::vector<std::thread> threads;
    std::atomic<int> successful_reads{0};
    std::atomic<int> successful_writes{0};
    std::atomic<int> exceptions_caught{0};
    
    // Initialize with known state
    std::map<std::string, std::shared_ptr<FrameworkData>> initial_params;
    for (int i = 0; i < 3; ++i) {
        initial_params["consistency_param_" + std::to_string(i)] = 
            std::make_shared<TestData>("initial_" + std::to_string(i));
    }
    node->setParam(initial_params);
    
    std::barrier sync_point(CONSISTENCY_THREADS);
    
    // Launch threads that perform concurrent operations
    for (int i = 0; i < CONSISTENCY_THREADS; ++i) {
        threads.emplace_back([this, i, &successful_reads, &successful_writes, &exceptions_caught,
                             &sync_point, CONSISTENCY_OPERATIONS]() {
            sync_point.arrive_and_wait();
            
            for (int j = 0; j < CONSISTENCY_OPERATIONS; ++j) {
                try {
                    if (i % 3 == 0) {
                        // Writer thread - update parameters
                        std::map<std::string, std::shared_ptr<FrameworkData>> params;
                        std::string suffix = std::to_string(i) + "_" + std::to_string(j);
                        
                        for (int k = 0; k < 3; ++k) {
                            params["consistency_param_" + std::to_string(k)] = 
                                std::make_shared<TestData>("writer_" + suffix);
                        }
                        node->setParam(params);
                        successful_writes++;
                        
                    } else {
                        // Reader thread - read parameters and verify no crashes
                        auto params = node->getParams();
                        
                        // Verify we can access all parameters without crashes
                        for (int k = 0; k < 3; ++k) {
                            std::string key = "consistency_param_" + std::to_string(k);
                            auto it = params.find(key);
                            
                            if (it != params.end()) {
                                auto test_data = std::dynamic_pointer_cast<TestData>(it->second);
                                if (test_data) {
                                    // Access the value to ensure memory is valid
                                    volatile auto value_length = test_data->value.length();
                                    (void)value_length; // Suppress unused variable warning
                                }
                            }
                        }
                        successful_reads++;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::microseconds(2));
                    
                } catch (const std::exception& e) {
                    exceptions_caught++;
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify thread safety - no exceptions should occur due to race conditions
    EXPECT_EQ(exceptions_caught.load(), 0) 
        << "Exceptions caught due to race conditions: " << exceptions_caught.load();
    EXPECT_GT(successful_reads.load(), 0) << "No successful read operations";
    EXPECT_GT(successful_writes.load(), 0) << "No successful write operations";
    
    // Verify final state is accessible
    auto final_params = node->getParams();
    EXPECT_EQ(final_params.size(), 3) << "Final parameter count is incorrect";
}