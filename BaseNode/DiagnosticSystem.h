#pragma once

#include "Logger.h"
#include "PerformanceProfiler.h"
#include "MemoryTracker.h"
#include "AsyncTracer.h"
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>

namespace AgenticFramework {

// Diagnostic configuration
struct DiagnosticConfig {
    bool enable_logging = true;
    bool enable_performance_profiling = true;
    bool enable_memory_tracking = false;  // Disabled by default due to overhead
    bool enable_async_tracing = true;
    
    LogLevel min_log_level = LogLevel::INFO;
    size_t max_recent_metrics = 1000;
    size_t max_completed_operations = 1000;
    
    std::string log_file_path = "";  // Empty means console only
    bool auto_flush_logs = true;
    
    // Performance monitoring thresholds
    std::chrono::milliseconds slow_operation_threshold{1000};
    std::chrono::milliseconds very_slow_operation_threshold{5000};
    
    // Memory monitoring thresholds
    size_t memory_leak_threshold_bytes = 1024 * 1024;  // 1MB
    size_t memory_usage_warning_threshold = 100 * 1024 * 1024;  // 100MB
};

// System health status
enum class SystemHealthStatus {
    HEALTHY,
    WARNING,
    CRITICAL,
    UNKNOWN
};

inline const char* to_string(SystemHealthStatus status) {
    switch (status) {
        case SystemHealthStatus::HEALTHY: return "HEALTHY";
        case SystemHealthStatus::WARNING: return "WARNING";
        case SystemHealthStatus::CRITICAL: return "CRITICAL";
        case SystemHealthStatus::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

// System health report
struct SystemHealthReport {
    SystemHealthStatus overall_status = SystemHealthStatus::UNKNOWN;
    std::chrono::system_clock::time_point timestamp;
    
    // Performance metrics
    size_t active_async_operations = 0;
    size_t failed_async_operations = 0;
    size_t slow_operations = 0;
    size_t very_slow_operations = 0;
    
    // Memory metrics
    size_t current_memory_usage = 0;
    size_t peak_memory_usage = 0;
    size_t memory_leaks = 0;
    
    // System metrics
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    
    SystemHealthReport() : timestamp(std::chrono::system_clock::now()) {}
};

// Main diagnostic system
class DiagnosticSystem {
private:
    DiagnosticConfig config_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> monitoring_enabled_{false};
    std::unique_ptr<std::thread> monitoring_thread_;
    std::atomic<bool> shutdown_requested_{false};
    
    static std::unique_ptr<DiagnosticSystem> instance_;
    static std::once_flag init_flag_;
    
    DiagnosticSystem() = default;
    
public:
    static DiagnosticSystem& getInstance();
    
    // Initialization and configuration
    void initialize(const DiagnosticConfig& config = DiagnosticConfig{});
    void shutdown();
    bool isInitialized() const { return initialized_.load(); }
    
    void updateConfig(const DiagnosticConfig& config);
    const DiagnosticConfig& getConfig() const { return config_; }
    
    // Component access
    Logger& getLogger() { return Logger::getInstance(); }
    PerformanceProfiler& getProfiler() { return PerformanceProfiler::getInstance(); }
    MemoryTracker& getMemoryTracker() { return MemoryTracker::getInstance(); }
    AsyncTracer& getAsyncTracer() { return AsyncTracer::getInstance(); }
    
    // Health monitoring
    void startHealthMonitoring(std::chrono::seconds interval = std::chrono::seconds(30));
    void stopHealthMonitoring();
    bool isHealthMonitoringEnabled() const { return monitoring_enabled_.load(); }
    
    SystemHealthReport generateHealthReport() const;
    void logHealthReport() const;
    
    // Comprehensive reporting
    std::string generateFullDiagnosticReport() const;
    void logFullDiagnosticReport() const;
    void saveFullDiagnosticReport(const std::string& filename) const;
    
    // Emergency diagnostics
    void emergencyDump(const std::string& reason = "Emergency diagnostic dump");
    void handleCriticalError(const std::string& error_message);
    
    // Utility methods
    void flushAllSystems();
    void clearAllData();
    void resetAllCounters();
    
    // Performance helpers
    template<typename F>
    auto measureAndLog(const std::string& operation_name, F&& func) -> decltype(func()) {
        if (!initialized_.load()) {
            return func();
        }
        
        PROFILE_SCOPE(operation_name);
        TRACE_ASYNC_OPERATION(operation_name);
        TRACK_MEMORY_SCOPE(operation_name);
        
        try {
            return func();
        } catch (const std::exception& e) {
            // LOG_ERROR("Operation failed: " + operation_name);
            throw;
        } catch (...) {
            // LOG_ERROR("Operation failed with unknown exception: " + operation_name);
            throw;
        }
    }
    
    // Callback registration for health monitoring
    using HealthCallback = std::function<void(const SystemHealthReport&)>;
    void registerHealthCallback(HealthCallback callback);
    
private:
    void monitoringLoop(std::chrono::seconds interval);
    SystemHealthStatus assessOverallHealth(const SystemHealthReport& report) const;
    void notifyHealthCallbacks(const SystemHealthReport& report);
    
    std::vector<HealthCallback> health_callbacks_;
    mutable std::mutex callbacks_mutex_;
};

// RAII diagnostic scope for comprehensive tracking
class DiagnosticScope {
private:
    std::string scope_name_;
    std::unique_ptr<ScopedTimer> timer_;
    std::unique_ptr<ScopedAsyncTracker> async_tracker_;
    std::unique_ptr<ScopedMemoryTracker> memory_tracker_;
    
public:
    explicit DiagnosticScope(const std::string& scope_name)
        : scope_name_(scope_name) {
        
        auto& diag = DiagnosticSystem::getInstance();
        if (!diag.isInitialized()) {
            return;
        }
        
        const auto& config = diag.getConfig();
        
        if (config.enable_performance_profiling) {
            timer_ = std::make_unique<ScopedTimer>(scope_name_, "", false);
        }
        
        if (config.enable_async_tracing) {
            async_tracker_ = std::make_unique<ScopedAsyncTracker>(scope_name_, "", true);
        }
        
        if (config.enable_memory_tracking) {
            memory_tracker_ = std::make_unique<ScopedMemoryTracker>(scope_name_);
        }
        
        if (config.enable_logging) {
            // LOG_TRACE("Entering diagnostic scope: " + scope_name_);
        }
    }
    
    ~DiagnosticScope() {
        auto& diag = DiagnosticSystem::getInstance();
        if (diag.isInitialized() && diag.getConfig().enable_logging) {
            // LOG_TRACE("Exiting diagnostic scope: " + scope_name_);
        }
    }
    
    void addMetadata(const std::string& key, const std::string& value) {
        if (async_tracker_) {
            async_tracker_->addMetadata(key, value);
        }
    }
    
    void updateState(AsyncOperationState state, const std::string& message = "") {
        if (async_tracker_) {
            async_tracker_->updateState(state, message);
        }
    }
};

} // namespace AgenticFramework

// Convenience macros for comprehensive diagnostics
#define DIAGNOSTIC_SCOPE(name) \
    AgenticFramework::DiagnosticScope _diag_scope(name)

#define DIAGNOSTIC_FUNCTION() \
    AgenticFramework::DiagnosticScope _diag_scope(__FUNCTION__)

#define MEASURE_AND_LOG(name, code) \
    AgenticFramework::DiagnosticSystem::getInstance().measureAndLog(name, [&]() { return code; })

#define EMERGENCY_DUMP(reason) \
    AgenticFramework::DiagnosticSystem::getInstance().emergencyDump(reason)

#define HANDLE_CRITICAL_ERROR(error) \
    AgenticFramework::DiagnosticSystem::getInstance().handleCriticalError(error)