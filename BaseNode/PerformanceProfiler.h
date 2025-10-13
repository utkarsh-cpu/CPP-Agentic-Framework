#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <atomic>
#include <vector>
#include <algorithm>
#include "Logger.h"

namespace AgenticFramework {

// Performance measurement data
struct PerformanceMetric {
    std::string name;
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;
    std::chrono::nanoseconds duration;
    std::thread::id thread_id;
    std::string context;
    
    PerformanceMetric(std::string n, std::string ctx = "")
        : name(std::move(n))
        , start_time(std::chrono::high_resolution_clock::now())
        , thread_id(std::this_thread::get_id())
        , context(std::move(ctx)) {}
    
    void finish() {
        end_time = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    }
    
    double milliseconds() const {
        return duration.count() / 1000000.0;
    }
    
    double microseconds() const {
        return duration.count() / 1000.0;
    }
};

// Statistics for accumulated metrics
struct MetricStatistics {
    std::string name;
    size_t count = 0;
    std::chrono::nanoseconds total_duration{0};
    std::chrono::nanoseconds min_duration{std::chrono::nanoseconds::max()};
    std::chrono::nanoseconds max_duration{std::chrono::nanoseconds::min()};
    std::chrono::nanoseconds avg_duration{0};
    
    void update(const PerformanceMetric& metric) {
        count++;
        total_duration += metric.duration;
        min_duration = std::min(min_duration, metric.duration);
        max_duration = std::max(max_duration, metric.duration);
        avg_duration = total_duration / count;
    }
    
    double avg_milliseconds() const {
        return avg_duration.count() / 1000000.0;
    }
    
    double min_milliseconds() const {
        return min_duration.count() / 1000000.0;
    }
    
    double max_milliseconds() const {
        return max_duration.count() / 1000000.0;
    }
    
    double total_milliseconds() const {
        return total_duration.count() / 1000000.0;
    }
};

// RAII timer for automatic measurement
class ScopedTimer {
private:
    std::unique_ptr<PerformanceMetric> metric_;
    bool auto_log_;
    
public:
    explicit ScopedTimer(const std::string& name, const std::string& context = "", bool auto_log = true)
        : metric_(std::make_unique<PerformanceMetric>(name, context))
        , auto_log_(auto_log) {}
    
    ~ScopedTimer();
    
    // Get current elapsed time without finishing the timer
    double elapsed_milliseconds() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - metric_->start_time);
        return elapsed.count() / 1000000.0;
    }
};

// Main performance profiler
class PerformanceProfiler {
private:
    std::unordered_map<std::string, MetricStatistics> statistics_;
    std::vector<PerformanceMetric> recent_metrics_;
    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{true};
    std::atomic<size_t> max_recent_metrics_{1000};
    
    static std::unique_ptr<PerformanceProfiler> instance_;
    static std::once_flag init_flag_;
    
    PerformanceProfiler() = default;
    
public:
    static PerformanceProfiler& getInstance();
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setMaxRecentMetrics(size_t max_metrics);
    
    void recordMetric(const PerformanceMetric& metric);
    
    MetricStatistics getStatistics(const std::string& name) const;
    std::vector<MetricStatistics> getAllStatistics() const;
    
    void clearStatistics();
    void clearRecentMetrics();
    
    // Get recent metrics for detailed analysis
    std::vector<PerformanceMetric> getRecentMetrics(size_t count = 100) const;
    
    // Generate performance report
    std::string generateReport() const;
    void logReport() const;
    
    // Measure function execution time
    template<typename F>
    auto measure(const std::string& name, F&& func) -> decltype(func()) {
        if (!enabled_.load()) {
            return func();
        }
        
        ScopedTimer timer(name, "", false);
        return func();
    }
    
    template<typename F>
    auto measure(const std::string& name, const std::string& context, F&& func) -> decltype(func()) {
        if (!enabled_.load()) {
            return func();
        }
        
        ScopedTimer timer(name, context, false);
        return func();
    }
};

} // namespace AgenticFramework

// Convenience macros for performance measurement
#define PROFILE_SCOPE(name) \
    AgenticFramework::ScopedTimer _timer(name, __FUNCTION__)

#define PROFILE_SCOPE_CONTEXT(name, context) \
    AgenticFramework::ScopedTimer _timer(name, context)

#define PROFILE_FUNCTION() \
    AgenticFramework::ScopedTimer _timer(__FUNCTION__)

#define MEASURE_EXECUTION(name, code) \
    AgenticFramework::PerformanceProfiler::getInstance().measure(name, [&]() { return code; })

#define MEASURE_EXECUTION_CONTEXT(name, context, code) \
    AgenticFramework::PerformanceProfiler::getInstance().measure(name, context, [&]() { return code; })