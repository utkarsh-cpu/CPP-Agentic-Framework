#include "../BaseNode/DiagnosticSystem.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

using namespace AgenticFramework;

// Example function that demonstrates comprehensive diagnostics
void exampleAsyncOperation(const std::string& operation_name, int duration_ms, bool should_fail = false) {
    DIAGNOSTIC_SCOPE(operation_name);
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    
    if (should_fail) {
        throw std::runtime_error("Simulated operation failure");
    }
    
    LOG_INFO("Completed operation: " + operation_name);
}

// Example of memory-intensive operation
void memoryIntensiveOperation() {
    DIAGNOSTIC_FUNCTION();
    
    std::vector<std::unique_ptr<std::vector<int>>> memory_blocks;
    
    // Allocate some memory blocks
    for (int i = 0; i < 10; ++i) {
        auto block = std::make_unique<std::vector<int>>(1000, i);
        memory_blocks.push_back(std::move(block));
        
        // Add some metadata to async tracking
        if (auto& tracer = AsyncTracer::getInstance(); tracer.isEnabled()) {
            // This would be done through the DIAGNOSTIC_SCOPE macro automatically
        }
    }
    
    LOG_INFO("Allocated memory blocks: " + std::to_string(memory_blocks.size()));
    
    // Simulate some processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Memory will be automatically freed when vector goes out of scope
}

// Example of performance measurement
double performanceTestOperation(int iterations) {
    PROFILE_FUNCTION();
    
    double result = 0.0;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    for (int i = 0; i < iterations; ++i) {
        result += dis(gen) * std::sin(i) * std::cos(i);
    }
    
    return result;
}

// Example health callback
void healthMonitoringCallback(const SystemHealthReport& report) {
    if (report.overall_status != SystemHealthStatus::HEALTHY) {
        std::cout << "Health Alert: System status is " << to_string(report.overall_status) << std::endl;
        
        if (report.active_async_operations > 50) {
            std::cout << "  - High number of active operations: " << report.active_async_operations << std::endl;
        }
        
        if (report.memory_leaks > 0) {
            std::cout << "  - Memory leaks detected: " << report.memory_leaks << " bytes" << std::endl;
        }
    }
}

int main() {
    std::cout << "=== Diagnostic System Example ===" << std::endl;
    
    // Configure diagnostic system
    DiagnosticConfig config;
    config.enable_logging = true;
    config.enable_performance_profiling = true;
    config.enable_memory_tracking = true;  // Enable for demonstration
    config.enable_async_tracing = true;
    config.min_log_level = LogLevel::DEBUG;
    config.log_file_path = "diagnostic_example.log";
    config.slow_operation_threshold = std::chrono::milliseconds(500);
    
    // Initialize diagnostic system
    auto& diagnostic = DiagnosticSystem::getInstance();
    diagnostic.initialize(config);
    
    // Register health monitoring callback
    diagnostic.registerHealthCallback(healthMonitoringCallback);
    
    // Start health monitoring
    diagnostic.startHealthMonitoring(std::chrono::seconds(5));
    
    LOG_INFO("Starting diagnostic system example");
    
    try {
        // Example 1: Basic operation with diagnostics
        {
            DIAGNOSTIC_SCOPE("basic_operation_example");
            LOG_INFO("Performing basic operation");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Example 2: Performance measurement
        {
            DIAGNOSTIC_SCOPE("performance_test");
            LOG_INFO("Starting performance test");
            
            auto result = MEASURE_AND_LOG("math_computation", 
                performanceTestOperation(100000)
            );
            
            LOG_INFO("Performance test result: " + std::to_string(result));
        }
        
        // Example 3: Memory tracking
        {
            DIAGNOSTIC_SCOPE("memory_test");
            LOG_INFO("Starting memory-intensive operation");
            memoryIntensiveOperation();
        }
        
        // Example 4: Async operations with different durations
        std::vector<std::thread> async_threads;
        
        for (int i = 0; i < 5; ++i) {
            async_threads.emplace_back([i]() {
                try {
                    std::string op_name = "async_operation_" + std::to_string(i);
                    int duration = 100 + (i * 200);  // Varying durations
                    bool should_fail = (i == 3);     // Make one operation fail
                    
                    exampleAsyncOperation(op_name, duration, should_fail);
                } catch (const std::exception& e) {
                    LOG_ERROR("Async operation failed: " + std::string(e.what()));
                }
            });
        }
        
        // Example 5: Long-running operation that will trigger warnings
        std::thread long_running_thread([]() {
            try {
                exampleAsyncOperation("long_running_operation", 2000);  // 2 seconds
            } catch (const std::exception& e) {
                LOG_ERROR("Long running operation failed: " + std::string(e.what()));
            }
        });
        
        // Wait for async operations to complete
        for (auto& thread : async_threads) {
            thread.join();
        }
        long_running_thread.join();
        
        // Example 6: Generate and display reports
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        LOG_INFO("Generating diagnostic reports...");
        
        // Performance report
        diagnostic.getProfiler().logReport();
        
        // Memory report
        if (config.enable_memory_tracking) {
            diagnostic.getMemoryTracker().logGlobalReport();
        }
        
        // Async operations report
        diagnostic.getAsyncTracer().logSummaryReport();
        diagnostic.getAsyncTracer().logFailedOperations();
        
        // Health report
        diagnostic.logHealthReport();
        
        // Full diagnostic report
        diagnostic.logFullDiagnosticReport();
        
        // Save comprehensive report to file
        diagnostic.saveFullDiagnosticReport("comprehensive_diagnostic_report.txt");
        
        // Example 7: Demonstrate emergency dump
        LOG_WARN("Simulating emergency condition...");
        diagnostic.emergencyDump("Demonstration of emergency dump functionality");
        
    } catch (const std::exception& e) {
        HANDLE_CRITICAL_ERROR(std::string("Example failed: ") + e.what());
    }
    
    LOG_INFO("Example completed successfully");
    
    // Clean shutdown
    std::this_thread::sleep_for(std::chrono::seconds(1));
    diagnostic.shutdown();
    
    std::cout << "=== Example Complete ===" << std::endl;
    std::cout << "Check 'diagnostic_example.log' and 'comprehensive_diagnostic_report.txt' for detailed logs" << std::endl;
    
    return 0;
}