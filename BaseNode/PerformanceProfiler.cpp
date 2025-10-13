#include "PerformanceProfiler.h"
#include <sstream>
#include <iomanip>

namespace AgenticFramework {

std::unique_ptr<PerformanceProfiler> PerformanceProfiler::instance_;
std::once_flag PerformanceProfiler::init_flag_;

// ScopedTimer destructor implementation
ScopedTimer::~ScopedTimer() {
    metric_->finish();
    if (auto_log_) {
        LOG_DEBUG("Timer completed: " + metric_->name);
    }
    PerformanceProfiler::getInstance().recordMetric(*metric_);
}

PerformanceProfiler& PerformanceProfiler::getInstance() {
    std::call_once(init_flag_, []() {
        instance_ = std::unique_ptr<PerformanceProfiler>(new PerformanceProfiler());
    });
    return *instance_;
}

void PerformanceProfiler::setEnabled(bool enabled) {
    enabled_.store(enabled);
}

bool PerformanceProfiler::isEnabled() const {
    return enabled_.load();
}

void PerformanceProfiler::setMaxRecentMetrics(size_t max_metrics) {
    max_recent_metrics_.store(max_metrics);
}

void PerformanceProfiler::recordMetric(const PerformanceMetric& metric) {
    if (!enabled_.load()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Update statistics
    statistics_[metric.name].name = metric.name;
    statistics_[metric.name].update(metric);
    
    // Store recent metric
    recent_metrics_.push_back(metric);
    
    // Maintain size limit for recent metrics
    size_t max_size = max_recent_metrics_.load();
    if (recent_metrics_.size() > max_size) {
        auto erase_count = static_cast<std::ptrdiff_t>(recent_metrics_.size() - max_size);
        recent_metrics_.erase(recent_metrics_.begin(), 
                             recent_metrics_.begin() + erase_count);
    }
}

MetricStatistics PerformanceProfiler::getStatistics(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = statistics_.find(name);
    return (it != statistics_.end()) ? it->second : MetricStatistics{};
}

std::vector<MetricStatistics> PerformanceProfiler::getAllStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MetricStatistics> result;
    result.reserve(statistics_.size());
    
    for (const auto& pair : statistics_) {
        result.push_back(pair.second);
    }
    
    // Sort by total time descending
    std::sort(result.begin(), result.end(), 
              [](const MetricStatistics& a, const MetricStatistics& b) {
                  return a.total_duration > b.total_duration;
              });
    
    return result;
}

void PerformanceProfiler::clearStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    statistics_.clear();
}

void PerformanceProfiler::clearRecentMetrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    recent_metrics_.clear();
}

std::vector<PerformanceMetric> PerformanceProfiler::getRecentMetrics(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (recent_metrics_.size() <= count) {
        return recent_metrics_;
    }
    
    return std::vector<PerformanceMetric>(
        recent_metrics_.end() - static_cast<std::ptrdiff_t>(count), recent_metrics_.end());
}

std::string PerformanceProfiler::generateReport() const {
    std::ostringstream report;
    
    report << "\n=== Performance Profiler Report ===\n";
    report << std::left << std::setw(30) << "Metric Name"
           << std::setw(10) << "Count"
           << std::setw(12) << "Total (ms)"
           << std::setw(12) << "Avg (ms)"
           << std::setw(12) << "Min (ms)"
           << std::setw(12) << "Max (ms)" << "\n";
    report << std::string(88, '-') << "\n";
    
    auto stats = getAllStatistics();
    for (const auto& stat : stats) {
        report << std::left << std::setw(30) << stat.name
               << std::setw(10) << stat.count
               << std::setw(12) << std::fixed << std::setprecision(3) << stat.total_milliseconds()
               << std::setw(12) << std::fixed << std::setprecision(3) << stat.avg_milliseconds()
               << std::setw(12) << std::fixed << std::setprecision(3) << stat.min_milliseconds()
               << std::setw(12) << std::fixed << std::setprecision(3) << stat.max_milliseconds() << "\n";
    }
    
    report << std::string(88, '-') << "\n";
    report << "Total metrics tracked: " << stats.size() << "\n";
    report << "Recent metrics stored: " << recent_metrics_.size() << "\n";
    report << "Profiler enabled: " << (enabled_.load() ? "Yes" : "No") << "\n";
    
    return report.str();
}

void PerformanceProfiler::logReport() const {
    LOG_INFO(generateReport());
}

} // namespace AgenticFramework