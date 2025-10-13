#include "DiagnosticSystem.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace AgenticFramework {

std::unique_ptr<DiagnosticSystem> DiagnosticSystem::instance_;
std::once_flag DiagnosticSystem::init_flag_;

DiagnosticSystem& DiagnosticSystem::getInstance() {
    std::call_once(init_flag_, []() {
        instance_ = std::unique_ptr<DiagnosticSystem>(new DiagnosticSystem());
    });
    return *instance_;
}

void DiagnosticSystem::initialize(const DiagnosticConfig& config) {
    if (initialized_.load()) {
        LOG_WARN("DiagnosticSystem already initialized");
        return;
    }
    
    config_ = config;
    
    // Initialize logging
    if (config_.enable_logging) {
        auto& logger = Logger::getInstance();
        logger.setMinLevel(config_.min_log_level);
        
        if (!config_.log_file_path.empty()) {
            try {
                logger.addSink(std::make_unique<FileSink>(config_.log_file_path));
                LOG_INFO("Added file sink: " + config_.log_file_path);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to add file sink: " + std::string(e.what()));
            }
        }
    }
    
    // Initialize performance profiler
    if (config_.enable_performance_profiling) {
        auto& profiler = PerformanceProfiler::getInstance();
        profiler.setEnabled(true);
        profiler.setMaxRecentMetrics(config_.max_recent_metrics);
    }
    
    // Initialize memory tracker
    if (config_.enable_memory_tracking) {
        auto& memory_tracker = MemoryTracker::getInstance();
        memory_tracker.setEnabled(true);
        LOG_WARN("Memory tracking enabled - this may impact performance");
    }
    
    // Initialize async tracer
    if (config_.enable_async_tracing) {
        auto& async_tracer = AsyncTracer::getInstance();
        async_tracer.setEnabled(true);
        async_tracer.setMaxCompletedOperations(config_.max_completed_operations);
    }
    
    initialized_.store(true);
    LOG_INFO("DiagnosticSystem initialized successfully");
}

void DiagnosticSystem::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    LOG_INFO("Shutting down DiagnosticSystem");
    
    // Stop health monitoring
    stopHealthMonitoring();
    
    // Generate final report
    logFullDiagnosticReport();
    
    // Flush all systems
    flushAllSystems();
    
    initialized_.store(false);
    LOG_INFO("DiagnosticSystem shutdown complete");
}

void DiagnosticSystem::updateConfig(const DiagnosticConfig& config) {
    config_ = config;
    
    if (initialized_.load()) {
        // Update component configurations
        if (config_.enable_logging) {
            Logger::getInstance().setMinLevel(config_.min_log_level);
        }
        
        PerformanceProfiler::getInstance().setEnabled(config_.enable_performance_profiling);
        if (config_.enable_performance_profiling) {
            PerformanceProfiler::getInstance().setMaxRecentMetrics(config_.max_recent_metrics);
        }
        
        MemoryTracker::getInstance().setEnabled(config_.enable_memory_tracking);
        
        AsyncTracer::getInstance().setEnabled(config_.enable_async_tracing);
        if (config_.enable_async_tracing) {
            AsyncTracer::getInstance().setMaxCompletedOperations(config_.max_completed_operations);
        }
        
        LOG_INFO("DiagnosticSystem configuration updated");
    }
}

void DiagnosticSystem::startHealthMonitoring(std::chrono::seconds interval) {
    if (monitoring_enabled_.load()) {
        LOG_WARN("Health monitoring already enabled");
        return;
    }
    
    shutdown_requested_.store(false);
    monitoring_enabled_.store(true);
    
    monitoring_thread_ = std::make_unique<std::thread>([this, interval]() {
        monitoringLoop(interval);
    });
    
    LOG_INFO("Health monitoring started");
}

void DiagnosticSystem::stopHealthMonitoring() {
    if (!monitoring_enabled_.load()) {
        return;
    }
    
    shutdown_requested_.store(true);
    monitoring_enabled_.store(false);
    
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
        monitoring_thread_.reset();
    }
    
    LOG_INFO("Health monitoring stopped");
}

void DiagnosticSystem::monitoringLoop(std::chrono::seconds interval) {
    while (!shutdown_requested_.load()) {
        try {
            auto report = generateHealthReport();
            
            // Log health status if not healthy
            if (report.overall_status != SystemHealthStatus::HEALTHY) {
                LOG_WARN("System health status: " + std::string(to_string(report.overall_status)));
                
                for (const auto& warning : report.warnings) {
                    LOG_WARN("Health warning: " + warning);
                }
                
                for (const auto& error : report.errors) {
                    LOG_ERROR("Health error: " + error);
                }
            }
            
            // Notify callbacks
            notifyHealthCallbacks(report);
            
            // Handle critical status
            if (report.overall_status == SystemHealthStatus::CRITICAL) {
                emergencyDump("Critical system health detected");
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error in health monitoring loop: " + std::string(e.what()));
        } catch (...) {
            LOG_ERROR("Unknown error in health monitoring loop");
        }
        
        // Sleep for the specified interval
        std::this_thread::sleep_for(interval);
    }
}

SystemHealthReport DiagnosticSystem::generateHealthReport() const {
    SystemHealthReport report;
    
    if (!initialized_.load()) {
        report.overall_status = SystemHealthStatus::UNKNOWN;
        report.errors.push_back("DiagnosticSystem not initialized");
        return report;
    }
    
    try {
        // Async operation metrics
        if (config_.enable_async_tracing) {
            auto& tracer = AsyncTracer::getInstance();
            report.active_async_operations = tracer.getActiveOperationCount();
            report.failed_async_operations = tracer.getFailedOperations().size();
            
            auto long_running = tracer.getLongRunningOperations(config_.slow_operation_threshold);
            report.slow_operations = long_running.size();
            
            auto very_long_running = tracer.getLongRunningOperations(config_.very_slow_operation_threshold);
            report.very_slow_operations = very_long_running.size();
            
            if (report.active_async_operations > 100) {
                report.warnings.push_back("High number of active async operations: " + 
                                        std::to_string(report.active_async_operations));
            }
            
            if (report.failed_async_operations > 10) {
                report.warnings.push_back("High number of failed async operations: " + 
                                        std::to_string(report.failed_async_operations));
            }
            
            if (report.very_slow_operations > 0) {
                report.errors.push_back("Very slow operations detected: " + 
                                      std::to_string(report.very_slow_operations));
            }
        }
        
        // Memory metrics
        if (config_.enable_memory_tracking) {
            auto& tracker = MemoryTracker::getInstance();
            auto stats = tracker.getGlobalStats();
            
            report.current_memory_usage = stats.getCurrentUsage();
            report.peak_memory_usage = stats.getPeakUsage();
            report.memory_leaks = stats.getLeakedBytes();
            
            if (report.current_memory_usage > config_.memory_usage_warning_threshold) {
                report.warnings.push_back("High memory usage: " + 
                                        std::to_string(report.current_memory_usage / (1024 * 1024)) + "MB");
            }
            
            if (report.memory_leaks > config_.memory_leak_threshold_bytes) {
                report.errors.push_back("Memory leaks detected: " + 
                                      std::to_string(report.memory_leaks) + " bytes");
            }
        }
        
        // Assess overall health
        report.overall_status = assessOverallHealth(report);
        
    } catch (const std::exception& e) {
        report.overall_status = SystemHealthStatus::UNKNOWN;
        report.errors.push_back("Error generating health report: " + std::string(e.what()));
    }
    
    return report;
}

SystemHealthStatus DiagnosticSystem::assessOverallHealth(const SystemHealthReport& report) const {
    if (!report.errors.empty()) {
        return SystemHealthStatus::CRITICAL;
    }
    
    if (!report.warnings.empty() || 
        report.very_slow_operations > 0 ||
        report.failed_async_operations > 5) {
        return SystemHealthStatus::WARNING;
    }
    
    return SystemHealthStatus::HEALTHY;
}

void DiagnosticSystem::logHealthReport() const {
    auto report = generateHealthReport();
    
    std::ostringstream oss;
    oss << "\n=== System Health Report ===\n";
    oss << "Overall Status: " << to_string(report.overall_status) << "\n";
    auto time_t_val = std::chrono::system_clock::to_time_t(report.timestamp);
    oss << "Timestamp: " << std::put_time(std::localtime(&time_t_val), 
                                         "%Y-%m-%d %H:%M:%S") << "\n";
    oss << "\nAsync Operations:\n";
    oss << "  Active: " << report.active_async_operations << "\n";
    oss << "  Failed: " << report.failed_async_operations << "\n";
    oss << "  Slow: " << report.slow_operations << "\n";
    oss << "  Very Slow: " << report.very_slow_operations << "\n";
    
    if (config_.enable_memory_tracking) {
        oss << "\nMemory Usage:\n";
        oss << "  Current: " << (report.current_memory_usage / 1024) << " KB\n";
        oss << "  Peak: " << (report.peak_memory_usage / 1024) << " KB\n";
        oss << "  Leaks: " << report.memory_leaks << " bytes\n";
    }
    
    if (!report.warnings.empty()) {
        oss << "\nWarnings:\n";
        for (const auto& warning : report.warnings) {
            oss << "  - " << warning << "\n";
        }
    }
    
    if (!report.errors.empty()) {
        oss << "\nErrors:\n";
        for (const auto& error : report.errors) {
            oss << "  - " << error << "\n";
        }
    }
    
    if (report.overall_status == SystemHealthStatus::HEALTHY) {
        LOG_INFO(oss.str());
    } else if (report.overall_status == SystemHealthStatus::WARNING) {
        LOG_WARN(oss.str());
    } else {
        LOG_ERROR(oss.str());
    }
}

std::string DiagnosticSystem::generateFullDiagnosticReport() const {
    std::ostringstream report;
    
    report << "=== COMPREHENSIVE DIAGNOSTIC REPORT ===\n";
    auto now_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    report << "Generated: " << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S") << "\n\n";
    
    // System health
    auto health = generateHealthReport();
    report << "System Health Status: " << to_string(health.overall_status) << "\n\n";
    
    // Performance profiler report
    if (config_.enable_performance_profiling) {
        report << PerformanceProfiler::getInstance().generateReport() << "\n";
    }
    
    // Memory tracker report
    if (config_.enable_memory_tracking) {
        report << MemoryTracker::getInstance().generateGlobalReport() << "\n";
    }
    
    // Async tracer report
    if (config_.enable_async_tracing) {
        report << AsyncTracer::getInstance().generateSummaryReport() << "\n";
    }
    
    report << "=== END DIAGNOSTIC REPORT ===\n";
    
    return report.str();
}

void DiagnosticSystem::logFullDiagnosticReport() const {
    LOG_INFO(generateFullDiagnosticReport());
}

void DiagnosticSystem::saveFullDiagnosticReport(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        
        file << generateFullDiagnosticReport();
        file.close();
        
        LOG_INFO("Diagnostic report saved to: " + filename);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save diagnostic report: " + std::string(e.what()));
    }
}

void DiagnosticSystem::emergencyDump(const std::string& reason) {
    LOG_FATAL("EMERGENCY DIAGNOSTIC DUMP: " + reason);
    
    // Generate timestamp for unique filename
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream filename;
    filename << "emergency_dump_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".txt";
    
    saveFullDiagnosticReport(filename.str());
    
    // Log critical information immediately
    logHealthReport();
    
    if (config_.enable_async_tracing) {
        AsyncTracer::getInstance().logActiveOperations();
        AsyncTracer::getInstance().logFailedOperations();
    }
    
    if (config_.enable_memory_tracking) {
        MemoryTracker::getInstance().logMemoryLeaks();
    }
    
    flushAllSystems();
}

void DiagnosticSystem::handleCriticalError(const std::string& error_message) {
    LOG_FATAL("CRITICAL ERROR: " + error_message);
    emergencyDump("Critical error occurred: " + error_message);
}

void DiagnosticSystem::flushAllSystems() {
    if (config_.enable_logging) {
        Logger::getInstance().flush();
    }
}

void DiagnosticSystem::clearAllData() {
    if (config_.enable_performance_profiling) {
        PerformanceProfiler::getInstance().clearStatistics();
        PerformanceProfiler::getInstance().clearRecentMetrics();
    }
    
    if (config_.enable_async_tracing) {
        AsyncTracer::getInstance().clearAllOperations();
    }
    
    LOG_INFO("All diagnostic data cleared");
}

void DiagnosticSystem::resetAllCounters() {
    clearAllData();
    LOG_INFO("All diagnostic counters reset");
}

void DiagnosticSystem::registerHealthCallback(HealthCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    health_callbacks_.push_back(std::move(callback));
}

void DiagnosticSystem::notifyHealthCallbacks(const SystemHealthReport& report) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    for (const auto& callback : health_callbacks_) {
        try {
            callback(report);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in health callback: " + std::string(e.what()));
        } catch (...) {
            LOG_ERROR("Unknown error in health callback");
        }
    }
}

} // namespace AgenticFramework