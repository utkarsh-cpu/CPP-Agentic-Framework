#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <future>
#include <filesystem>
#include "../BaseNode/DiagnosticSystem.h"
#include "../BaseNode/Logger.h"
#include "../BaseNode/PerformanceProfiler.h"
#include "../BaseNode/MemoryTracker.h"
#include "../BaseNode/AsyncTracer.h"

using namespace AgenticFramework;
using namespace std::chrono_literals;

// Custom log sink for testing
class TestLogSink : public LogSink {
private:
    std::vector<LogEntry> captured_entries_;
    mutable std::mutex mutex_;

public:
    void write(const LogEntry& entry) override {
        std::lock_guard<std::mutex> lock(mutex_);
        captured_entries_.push_back(entry);
    }
    
    void flush() override {
        // No-op for testing
    }
    
    std::vector<LogEntry> getCapturedEntries() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return captured_entries_;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        captured_entries_.clear();
    }
    
    size_t getEntryCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return captured_entries_.size();
    }
    
    LogEntry getLastEntry() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (captured_entries_.empty()) {
            throw std::runtime_error("No log entries captured");
        }
        return captured_entries_.back();
    }
};

class DiagnosticSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test log sink
        test_sink_ = std::make_shared<TestLogSink>();
        
        // Initialize diagnostic system if not already initialized
        auto& diag = DiagnosticSystem::getInstance();
        if (!diag.isInitialized()) {
            DiagnosticConfig config;
            config.enable_logging = true;
            config.enable_performance_profiling = true;
            config.enable_memory_tracking = true;
            config.enable_async_tracing = true;
            config.min_log_level = LogLevel::TRACE;
            config.max_recent_metrics = 100;
            config.max_completed_operations = 100;
            
            diag.initialize(config);
        }
        
        // Clear all diagnostic data for clean test state
        diag.clearAllData();
        diag.resetAllCounters();
        
        // Add our test sink to capture log output
        auto test_sink_copy = std::make_unique<TestLogSink>();
        test_sink_ = std::shared_ptr<TestLogSink>(test_sink_copy.get(), [](TestLogSink*){});
        Logger::getInstance().addSink(std::move(test_sink_copy));
    }
    
    void TearDown() override {
        // Clean up any test files
        std::filesystem::remove("test_log.txt");
        std::filesystem::remove("test_diagnostic_report.txt");
        
        // Note: We don't shutdown the diagnostic system here to avoid hanging
        // The singleton will be cleaned up at program exit
    }
    
    std::shared_ptr<TestLogSink> test_sink_;
};

// Test Suite 1: Logging Output Formatting and Filtering
class LoggingTest : public DiagnosticSystemTest {
protected:
    void SetUp() override {
        DiagnosticSystemTest::SetUp();
        test_sink_->clear();
    }
};

TEST_F(LoggingTest, LogLevelFiltering) {
    auto& logger = Logger::getInstance();
    
    // Set minimum level to WARN
    logger.setMinLevel(LogLevel::WARN);
    
    // Log messages at different levels
    logger.log(LogLevel::TRACE, "Trace message");
    logger.log(LogLevel::DEBUG, "Debug message");
    logger.log(LogLevel::INFO, "Info message");
    logger.log(LogLevel::WARN, "Warning message");
    logger.log(LogLevel::ERROR, "Error message");
    logger.log(LogLevel::FATAL, "Fatal message");
    
    // Only WARN, ERROR, and FATAL should be captured
    auto entries = test_sink_->getCapturedEntries();
    EXPECT_EQ(entries.size(), 3);
    
    if (entries.size() >= 3) {
        EXPECT_EQ(entries[0].level, LogLevel::WARN);
        EXPECT_EQ(entries[0].message, "Warning message");
        EXPECT_EQ(entries[1].level, LogLevel::ERROR);
        EXPECT_EQ(entries[1].message, "Error message");
        EXPECT_EQ(entries[2].level, LogLevel::FATAL);
        EXPECT_EQ(entries[2].message, "Fatal message");
    }
}

TEST_F(LoggingTest, LogEntryFormatting) {
    auto& logger = Logger::getInstance();
    logger.setMinLevel(LogLevel::TRACE);
    
    // Log a message with context and file information
    logger.log(LogLevel::INFO, "Test message", "TestContext", "test_file.cpp", 42, "testFunction");
    
    auto entries = test_sink_->getCapturedEntries();
    ASSERT_EQ(entries.size(), 1);
    
    const auto& entry = entries[0];
    EXPECT_EQ(entry.level, LogLevel::INFO);
    EXPECT_EQ(entry.message, "Test message");
    EXPECT_EQ(entry.context, "TestContext");
    EXPECT_EQ(entry.file, "test_file.cpp");
    EXPECT_EQ(entry.line, 42);
    EXPECT_EQ(entry.function, "testFunction");
    
    // Verify timestamp is recent (within last second)
    auto now = std::chrono::system_clock::now();
    auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(now - entry.timestamp);
    EXPECT_LE(time_diff.count(), 1);
}

TEST_F(LoggingTest, ThreadSafeLogging) {
    auto& logger = Logger::getInstance();
    logger.setMinLevel(LogLevel::INFO);
    
    const int num_threads = 10;
    const int messages_per_thread = 50;
    std::vector<std::future<void>> futures;
    
    // Launch multiple threads logging concurrently
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [&logger, i]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                logger.log(LogLevel::INFO, "Thread " + std::to_string(i) + " Message " + std::to_string(j));
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    // Verify all messages were logged
    auto entries = test_sink_->getCapturedEntries();
    EXPECT_EQ(entries.size(), num_threads * messages_per_thread);
    
    // Verify no corruption in messages
    std::set<std::string> unique_messages;
    for (const auto& entry : entries) {
        unique_messages.insert(entry.message);
    }
    EXPECT_EQ(unique_messages.size(), num_threads * messages_per_thread);
}

TEST_F(LoggingTest, FileSinkFunctionality) {
    const std::string test_file = "test_log.txt";
    
    // Create a file sink
    auto file_sink = std::make_unique<FileSink>(test_file);
    
    // Log some messages
    LogEntry entry1(LogLevel::INFO, "First message");
    LogEntry entry2(LogLevel::ERROR, "Second message");
    
    file_sink->write(entry1);
    file_sink->write(entry2);
    file_sink->flush();
    
    // Reset the sink to close the file
    file_sink.reset();
    
    // Verify file contents
    std::ifstream file(test_file);
    ASSERT_TRUE(file.is_open());
    
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    EXPECT_EQ(lines.size(), 2);
    EXPECT_THAT(lines[0], testing::HasSubstr("First message"));
    EXPECT_THAT(lines[0], testing::HasSubstr("[INFO]"));
    EXPECT_THAT(lines[1], testing::HasSubstr("Second message"));
    EXPECT_THAT(lines[1], testing::HasSubstr("[ERROR]"));
}

// Test Suite 2: Performance Measurement Accuracy
class PerformanceTest : public DiagnosticSystemTest {
protected:
    void SetUp() override {
        DiagnosticSystemTest::SetUp();
        PerformanceProfiler::getInstance().clearStatistics();
        PerformanceProfiler::getInstance().clearRecentMetrics();
    }
};

TEST_F(PerformanceTest, BasicTimingAccuracy) {
    auto& profiler = PerformanceProfiler::getInstance();
    profiler.setEnabled(true);
    
    const auto sleep_duration = 100ms;
    const double tolerance_ms = 50.0; // Allow 50ms tolerance
    
    {
        ScopedTimer timer("test_operation", "", false);
        std::this_thread::sleep_for(sleep_duration);
    }
    
    auto stats = profiler.getStatistics("test_operation");
    EXPECT_EQ(stats.count, 1);
    EXPECT_NEAR(stats.avg_milliseconds(), sleep_duration.count(), tolerance_ms);
    EXPECT_NEAR(stats.min_milliseconds(), sleep_duration.count(), tolerance_ms);
    EXPECT_NEAR(stats.max_milliseconds(), sleep_duration.count(), tolerance_ms);
}

TEST_F(PerformanceTest, MultipleOperationStatistics) {
    auto& profiler = PerformanceProfiler::getInstance();
    profiler.setEnabled(true);
    
    const std::string operation_name = "multi_test";
    const int num_operations = 5;
    std::vector<std::chrono::milliseconds> sleep_durations = {10ms, 20ms, 30ms, 40ms, 50ms};
    
    // Perform multiple timed operations
    for (int i = 0; i < num_operations; ++i) {
        ScopedTimer timer(operation_name, "", false);
        std::this_thread::sleep_for(sleep_durations[static_cast<size_t>(i)]);
    }
    
    auto stats = profiler.getStatistics(operation_name);
    EXPECT_EQ(stats.count, num_operations);
    
    // Verify min and max are reasonable
    EXPECT_GE(stats.min_milliseconds(), 5.0);  // At least 5ms (allowing for timing variance)
    EXPECT_LE(stats.max_milliseconds(), 100.0); // At most 100ms (allowing for timing variance)
    
    // Verify average is in expected range
    double expected_avg = (10 + 20 + 30 + 40 + 50) / 5.0;
    EXPECT_NEAR(stats.avg_milliseconds(), expected_avg, 20.0); // 20ms tolerance
}

TEST_F(PerformanceTest, ConcurrentTimingAccuracy) {
    auto& profiler = PerformanceProfiler::getInstance();
    profiler.setEnabled(true);
    
    const int num_threads = 4;
    const int operations_per_thread = 10;
    std::vector<std::future<void>> futures;
    
    // Launch concurrent timing operations
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string op_name = "concurrent_op_" + std::to_string(i);
                ScopedTimer timer(op_name, "", false);
                std::this_thread::sleep_for(std::chrono::milliseconds(10 + j));
            }
        }));
    }
    
    // Wait for completion
    for (auto& future : futures) {
        future.wait();
    }
    
    // Verify statistics for each thread's operations
    for (int i = 0; i < num_threads; ++i) {
        std::string op_name = "concurrent_op_" + std::to_string(i);
        auto stats = profiler.getStatistics(op_name);
        EXPECT_EQ(stats.count, operations_per_thread);
        EXPECT_GT(stats.total_milliseconds(), 0);
    }
}

TEST_F(PerformanceTest, RecentMetricsTracking) {
    auto& profiler = PerformanceProfiler::getInstance();
    profiler.setEnabled(true);
    profiler.setMaxRecentMetrics(5); // Limit to 5 recent metrics
    
    // Generate more metrics than the limit
    for (int i = 0; i < 10; ++i) {
        ScopedTimer timer("recent_test_" + std::to_string(i), "", false);
        std::this_thread::sleep_for(1ms);
    }
    
    auto recent = profiler.getRecentMetrics(10);
    EXPECT_EQ(recent.size(), 5); // Should be limited to max
    
    // Verify the most recent metrics are kept
    for (size_t i = 0; i < recent.size(); ++i) {
        std::string expected_name = "recent_test_" + std::to_string(5 + i);
        EXPECT_EQ(recent[i].name, expected_name);
    }
}

TEST_F(PerformanceTest, PerformanceReportGeneration) {
    auto& profiler = PerformanceProfiler::getInstance();
    profiler.setEnabled(true);
    
    // Generate some test metrics
    {
        ScopedTimer timer("fast_operation", "", false);
        std::this_thread::sleep_for(5ms);
    }
    
    {
        ScopedTimer timer("slow_operation", "", false);
        std::this_thread::sleep_for(50ms);
    }
    
    std::string report = profiler.generateReport();
    
    // Verify report contains expected information
    EXPECT_THAT(report, testing::HasSubstr("Performance Profiler Report"));
    EXPECT_THAT(report, testing::HasSubstr("fast_operation"));
    EXPECT_THAT(report, testing::HasSubstr("slow_operation"));
    EXPECT_THAT(report, testing::HasSubstr("Count"));
    EXPECT_THAT(report, testing::HasSubstr("Total (ms)"));
    EXPECT_THAT(report, testing::HasSubstr("Avg (ms)"));
}

// Test Suite 3: Memory Leak Detection Capabilities
class MemoryTrackingTest : public DiagnosticSystemTest {
protected:
    void SetUp() override {
        DiagnosticSystemTest::SetUp();
        auto& tracker = MemoryTracker::getInstance();
        tracker.setEnabled(true);
        
        // Clear any existing pools and reset counters
        for (const auto& pool_name : tracker.getPoolNames()) {
            tracker.removePool(pool_name);
        }
    }
};

TEST_F(MemoryTrackingTest, BasicAllocationTracking) {
    auto& tracker = MemoryTracker::getInstance();
    auto& pool = tracker.getPool("test_pool");
    pool.setTrackAllocations(true);
    
    // Simulate allocations
    void* ptr1 = malloc(100);
    void* ptr2 = malloc(200);
    
    pool.recordAllocation(ptr1, 100, "test.cpp", 10, "testFunc");
    pool.recordAllocation(ptr2, 200, "test.cpp", 20, "testFunc");
    
    auto stats = pool.getStats();
    EXPECT_EQ(stats.getCurrentUsage(), 300);
    EXPECT_EQ(stats.getAllocationCount(), 2);
    EXPECT_EQ(stats.getLeakedBytes(), 300); // No deallocations yet
    
    // Record deallocations
    pool.recordDeallocation(ptr1);
    pool.recordDeallocation(ptr2);
    
    stats = pool.getStats();
    EXPECT_EQ(stats.getCurrentUsage(), 0);
    EXPECT_EQ(stats.getDeallocationCount(), 2);
    EXPECT_EQ(stats.getLeakedBytes(), 0);
    
    free(ptr1);
    free(ptr2);
}

TEST_F(MemoryTrackingTest, MemoryLeakDetection) {
    auto& tracker = MemoryTracker::getInstance();
    auto& pool = tracker.getPool("leak_test_pool");
    pool.setTrackAllocations(true);
    
    // Simulate memory leaks
    void* leaked_ptr1 = malloc(500);
    void* leaked_ptr2 = malloc(1000);
    void* freed_ptr = malloc(300);
    
    pool.recordAllocation(leaked_ptr1, 500, "leak_test.cpp", 15, "leakyFunction");
    pool.recordAllocation(leaked_ptr2, 1000, "leak_test.cpp", 25, "anotherLeakyFunction");
    pool.recordAllocation(freed_ptr, 300, "leak_test.cpp", 35, "goodFunction");
    
    // Only free one allocation
    pool.recordDeallocation(freed_ptr);
    free(freed_ptr);
    
    auto stats = pool.getStats();
    EXPECT_EQ(stats.getLeakedBytes(), 1500); // 500 + 1000
    EXPECT_EQ(stats.getLeakedAllocations(), 2);
    
    // Get active allocations (leaks)
    auto leaks = pool.getActiveAllocations();
    EXPECT_EQ(leaks.size(), 2);
    
    // Verify leak information
    bool found_500_byte_leak = false;
    bool found_1000_byte_leak = false;
    
    for (const auto& leak : leaks) {
        if (leak.size == 500) {
            found_500_byte_leak = true;
            EXPECT_EQ(leak.line, 15);
            EXPECT_EQ(leak.function, "leakyFunction");
        } else if (leak.size == 1000) {
            found_1000_byte_leak = true;
            EXPECT_EQ(leak.line, 25);
            EXPECT_EQ(leak.function, "anotherLeakyFunction");
        }
    }
    
    EXPECT_TRUE(found_500_byte_leak);
    EXPECT_TRUE(found_1000_byte_leak);
    
    // Clean up
    free(leaked_ptr1);
    free(leaked_ptr2);
}

TEST_F(MemoryTrackingTest, GlobalMemoryTracking) {
    auto& tracker = MemoryTracker::getInstance();
    
    // Record global allocations
    tracker.recordGlobalAllocation(1000);
    tracker.recordGlobalAllocation(2000);
    tracker.recordGlobalAllocation(500);
    
    auto stats = tracker.getGlobalStats();
    EXPECT_EQ(stats.getCurrentUsage(), 3500);
    EXPECT_EQ(stats.getAllocationCount(), 3);
    
    // Record some deallocations
    tracker.recordGlobalDeallocation(1000);
    tracker.recordGlobalDeallocation(500);
    
    stats = tracker.getGlobalStats();
    EXPECT_EQ(stats.getCurrentUsage(), 2000);
    EXPECT_EQ(stats.getDeallocationCount(), 2);
    EXPECT_EQ(stats.getLeakedBytes(), 2000);
}

TEST_F(MemoryTrackingTest, MemoryLeakReportGeneration) {
    auto& tracker = MemoryTracker::getInstance();
    auto& pool = tracker.getPool("report_test_pool");
    pool.setTrackAllocations(true);
    
    // Create some leaks
    void* leak1 = malloc(256);
    void* leak2 = malloc(512);
    
    pool.recordAllocation(leak1, 256, "report_test.cpp", 100, "reportTestFunc1");
    pool.recordAllocation(leak2, 512, "report_test.cpp", 200, "reportTestFunc2");
    
    std::string leak_report = pool.generateLeakReport();
    
    // Verify report contains leak information
    EXPECT_THAT(leak_report, testing::HasSubstr("Memory Leak Report"));
    EXPECT_THAT(leak_report, testing::HasSubstr("report_test_pool"));
    EXPECT_THAT(leak_report, testing::HasSubstr("Total leaked allocations: 2"));
    EXPECT_THAT(leak_report, testing::HasSubstr("Total leaked bytes: 768"));
    EXPECT_THAT(leak_report, testing::HasSubstr("256"));
    EXPECT_THAT(leak_report, testing::HasSubstr("512"));
    EXPECT_THAT(leak_report, testing::HasSubstr("reportTestFunc1"));
    EXPECT_THAT(leak_report, testing::HasSubstr("reportTestFunc2"));
    
    // Clean up
    free(leak1);
    free(leak2);
}

TEST_F(MemoryTrackingTest, ScopedMemoryTracking) {
    auto& tracker = MemoryTracker::getInstance();
    
    size_t initial_usage = tracker.getGlobalStats().getCurrentUsage();
    
    {
        ScopedMemoryTracker scoped_tracker("test_scope");
        
        // Simulate some memory operations within scope
        tracker.recordGlobalAllocation(1000);
        tracker.recordGlobalAllocation(500);
        tracker.recordGlobalDeallocation(200);
        
        // Scoped tracker should detect the net change when it goes out of scope
    }
    
    size_t final_usage = tracker.getGlobalStats().getCurrentUsage();
    EXPECT_EQ(final_usage - initial_usage, 1300); // 1000 + 500 - 200
}

TEST_F(MemoryTrackingTest, ConcurrentMemoryTracking) {
    auto& tracker = MemoryTracker::getInstance();
    auto& pool = tracker.getPool("concurrent_test_pool");
    pool.setTrackAllocations(true);
    
    const int num_threads = 4;
    const int allocations_per_thread = 25;
    std::vector<std::future<void>> futures;
    
    // Launch concurrent allocation tracking
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [&pool, i]() {
            std::vector<void*> ptrs;
            
            // Allocate
            for (int j = 0; j < allocations_per_thread; ++j) {
                void* ptr = malloc(static_cast<size_t>(100 + j));
                ptrs.push_back(ptr);
                pool.recordAllocation(ptr, static_cast<size_t>(100 + j), "concurrent_test.cpp", 
                                    i * 100 + j, "concurrentTestFunc");
            }
            
            // Deallocate half
            for (int j = 0; j < allocations_per_thread / 2; ++j) {
                pool.recordDeallocation(ptrs[static_cast<size_t>(j)]);
                free(ptrs[static_cast<size_t>(j)]);
            }
            
            // Clean up remaining (but don't record deallocation to simulate leaks)
            for (int j = allocations_per_thread / 2; j < allocations_per_thread; ++j) {
                free(ptrs[static_cast<size_t>(j)]);
            }
        }));
    }
    
    // Wait for completion
    for (auto& future : futures) {
        future.wait();
    }
    
    auto stats = pool.getStats();
    EXPECT_EQ(stats.getAllocationCount(), num_threads * allocations_per_thread);
    EXPECT_EQ(stats.getDeallocationCount(), num_threads * (allocations_per_thread / 2));
    
    // Should have leaks from the unrecorded deallocations
    EXPECT_GT(stats.getLeakedBytes(), 0);
    EXPECT_GT(stats.getLeakedAllocations(), 0);
}

// Test Suite 4: Integrated Diagnostic System Tests
class IntegratedDiagnosticTest : public DiagnosticSystemTest {
protected:
    void SetUp() override {
        DiagnosticSystemTest::SetUp();
        test_sink_->clear();
    }
};

TEST_F(IntegratedDiagnosticTest, SystemHealthReporting) {
    auto& diag = DiagnosticSystem::getInstance();
    
    // Generate some activity to test health reporting
    {
        DIAGNOSTIC_SCOPE("test_operation");
        std::this_thread::sleep_for(10ms);
    }
    
    auto health_report = diag.generateHealthReport();
    
    EXPECT_NE(health_report.overall_status, SystemHealthStatus::UNKNOWN);
    EXPECT_GT(health_report.timestamp.time_since_epoch().count(), 0);
}

TEST_F(IntegratedDiagnosticTest, ComprehensiveDiagnosticReport) {
    auto& diag = DiagnosticSystem::getInstance();
    
    // Generate some diagnostic activity
    {
        DIAGNOSTIC_SCOPE("comprehensive_test");
        std::this_thread::sleep_for(5ms);
    }
    
    std::string full_report = diag.generateFullDiagnosticReport();
    
    // Verify report contains sections from all diagnostic components
    EXPECT_THAT(full_report, testing::HasSubstr("COMPREHENSIVE DIAGNOSTIC REPORT"));
    EXPECT_THAT(full_report, testing::HasSubstr("System Health Status"));
    EXPECT_GT(full_report.length(), 100); // Should be substantial
}

TEST_F(IntegratedDiagnosticTest, EmergencyDumpFunctionality) {
    auto& diag = DiagnosticSystem::getInstance();
    
    // Trigger emergency dump
    diag.emergencyDump("Test emergency dump");
    
    // Verify emergency dump was logged
    auto entries = test_sink_->getCapturedEntries();
    bool found_emergency_log = false;
    
    for (const auto& entry : entries) {
        if (entry.level == LogLevel::FATAL && 
            entry.message.find("EMERGENCY DIAGNOSTIC DUMP") != std::string::npos) {
            found_emergency_log = true;
            break;
        }
    }
    
    EXPECT_TRUE(found_emergency_log);
}

TEST_F(IntegratedDiagnosticTest, DiagnosticScopeIntegration) {
    auto& profiler = PerformanceProfiler::getInstance();
    auto& tracer = AsyncTracer::getInstance();
    auto& tracker = MemoryTracker::getInstance();
    
    profiler.clearStatistics();
    tracer.clearAllOperations();
    
    {
        DIAGNOSTIC_SCOPE("integrated_scope_test");
        std::this_thread::sleep_for(20ms);
        
        // Simulate some memory activity
        tracker.recordGlobalAllocation(1000);
        tracker.recordGlobalDeallocation(500);
    }
    
    // Verify all diagnostic systems captured the activity
    auto perf_stats = profiler.getStatistics("integrated_scope_test");
    EXPECT_EQ(perf_stats.count, 1);
    EXPECT_GT(perf_stats.avg_milliseconds(), 15.0); // Should be at least 15ms
    
    // Check that async operations were tracked
    EXPECT_GE(tracer.getCompletedOperationCount(), 1);
}

// Main test runner
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}