#include "AsyncTracer.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace AgenticFramework {

std::unique_ptr<AsyncTracer> AsyncTracer::instance_;
std::once_flag AsyncTracer::init_flag_;

// ScopedAsyncTracker implementation
ScopedAsyncTracker::ScopedAsyncTracker(const std::string& operation_name, 
                                      const std::string& context, bool auto_complete)
    : auto_complete_(auto_complete) {
    if (AsyncTracer::getInstance().isEnabled()) {
        operation_id_ = AsyncTracer::getInstance().startOperation(operation_name, context);
    }
}

ScopedAsyncTracker::~ScopedAsyncTracker() {
    if (!operation_id_.empty() && auto_complete_) {
        AsyncTracer::getInstance().completeOperation(operation_id_);
    }
}

void ScopedAsyncTracker::updateState(AsyncOperationState state, const std::string& message) {
    if (!operation_id_.empty()) {
        AsyncTracer::getInstance().updateOperationState(operation_id_, state, message);
    }
}

void ScopedAsyncTracker::addMetadata(const std::string& key, const std::string& value) {
    if (!operation_id_.empty()) {
        AsyncTracer::getInstance().addOperationMetadata(operation_id_, key, value);
    }
}

void ScopedAsyncTracker::setError(const std::string& error_message) {
    if (!operation_id_.empty()) {
        AsyncTracer::getInstance().failOperation(operation_id_, error_message);
        auto_complete_ = false; // Don't auto-complete on error
    }
}

// AsyncTracer implementation
AsyncTracer& AsyncTracer::getInstance() {
    std::call_once(init_flag_, []() {
        instance_ = std::unique_ptr<AsyncTracer>(new AsyncTracer());
    });
    return *instance_;
}

void AsyncTracer::setEnabled(bool enabled) {
    enabled_.store(enabled);
    if (enabled) {
        LOG_INFO("Async operation tracing enabled");
    } else {
        LOG_INFO("Async operation tracing disabled");
    }
}

bool AsyncTracer::isEnabled() const {
    return enabled_.load();
}

void AsyncTracer::setMaxCompletedOperations(size_t max_operations) {
    max_completed_operations_.store(max_operations);
}

std::string AsyncTracer::generateOperationId() {
    auto counter = operation_counter_.fetch_add(1);
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::ostringstream oss;
    oss << "async_" << timestamp << "_" << counter;
    return oss.str();
}

std::string AsyncTracer::startOperation(const std::string& operation_name, const std::string& context) {
    if (!enabled_.load()) {
        return "";
    }
    
    std::string operation_id = generateOperationId();
    auto trace = std::make_unique<AsyncOperationTrace>(operation_id, operation_name, context);
    
    std::lock_guard<std::mutex> lock(mutex_);
    active_operations_[operation_id] = std::move(trace);
    
    LOG_TRACE("Started async operation: " + operation_id);
    return operation_id;
}

void AsyncTracer::updateOperationState(const std::string& operation_id, AsyncOperationState state, 
                                      const std::string& message) {
    if (!enabled_.load() || operation_id.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_operations_.find(operation_id);
    if (it != active_operations_.end()) {
        it->second->updateState(state, message);
        LOG_TRACE("Updated async operation " + operation_id + " state to " + to_string(state));
    }
}

void AsyncTracer::addOperationMetadata(const std::string& operation_id, const std::string& key, 
                                      const std::string& value) {
    if (!enabled_.load() || operation_id.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_operations_.find(operation_id);
    if (it != active_operations_.end()) {
        it->second->addMetadata(key, value);
    }
}

void AsyncTracer::completeOperation(const std::string& operation_id) {
    if (!enabled_.load() || operation_id.empty()) {
        return;
    }
    
    updateOperationState(operation_id, AsyncOperationState::COMPLETED);
    moveToCompleted(operation_id);
    LOG_TRACE("Completed async operation: " + operation_id);
}

void AsyncTracer::failOperation(const std::string& operation_id, const std::string& error_message) {
    if (!enabled_.load() || operation_id.empty()) {
        return;
    }
    
    updateOperationState(operation_id, AsyncOperationState::FAILED, error_message);
    moveToCompleted(operation_id);
    LOG_WARN("Failed async operation " + operation_id + ": " + error_message);
}

void AsyncTracer::cancelOperation(const std::string& operation_id) {
    if (!enabled_.load() || operation_id.empty()) {
        return;
    }
    
    updateOperationState(operation_id, AsyncOperationState::CANCELLED);
    moveToCompleted(operation_id);
    LOG_INFO("Cancelled async operation: " + operation_id);
}

void AsyncTracer::timeoutOperation(const std::string& operation_id) {
    if (!enabled_.load() || operation_id.empty()) {
        return;
    }
    
    updateOperationState(operation_id, AsyncOperationState::TIMEOUT);
    moveToCompleted(operation_id);
    LOG_WARN("Timeout async operation: " + operation_id);
}

void AsyncTracer::addDependency(const std::string& parent_id, const std::string& child_id, 
                               const std::string& relationship_type) {
    if (!enabled_.load() || parent_id.empty() || child_id.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    dependencies_.emplace_back(parent_id, child_id, relationship_type);
    LOG_TRACE("Added dependency: " + parent_id + " " + relationship_type + " " + child_id);
}

void AsyncTracer::moveToCompleted(const std::string& operation_id) {
    auto it = active_operations_.find(operation_id);
    if (it != active_operations_.end()) {
        completed_operations_.push_back(std::move(it->second));
        active_operations_.erase(it);
        
        // Maintain size limit for completed operations
        size_t max_size = max_completed_operations_.load();
        if (completed_operations_.size() > max_size) {
            auto erase_count = static_cast<std::ptrdiff_t>(completed_operations_.size() - max_size);
            completed_operations_.erase(completed_operations_.begin(), 
                                       completed_operations_.begin() + erase_count);
        }
    }
}

std::shared_ptr<AsyncOperationTrace> AsyncTracer::getOperation(const std::string& operation_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check active operations first
    auto it = active_operations_.find(operation_id);
    if (it != active_operations_.end()) {
        return std::shared_ptr<AsyncOperationTrace>(it->second.get(), [](AsyncOperationTrace*){});
    }
    
    // Check completed operations
    auto comp_it = std::find_if(completed_operations_.begin(), completed_operations_.end(),
                               [&operation_id](const std::unique_ptr<AsyncOperationTrace>& trace) {
                                   return trace->operation_id == operation_id;
                               });
    
    if (comp_it != completed_operations_.end()) {
        return std::shared_ptr<AsyncOperationTrace>(comp_it->get(), [](AsyncOperationTrace*){});
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<AsyncOperationTrace>> AsyncTracer::getActiveOperations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<AsyncOperationTrace>> result;
    result.reserve(active_operations_.size());
    
    for (const auto& pair : active_operations_) {
        result.push_back(std::shared_ptr<AsyncOperationTrace>(pair.second.get(), [](AsyncOperationTrace*){}));
    }
    
    return result;
}

std::vector<std::shared_ptr<AsyncOperationTrace>> AsyncTracer::getCompletedOperations(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<AsyncOperationTrace>> result;
    
    size_t start_idx = (completed_operations_.size() > count) ? completed_operations_.size() - count : 0;
    result.reserve(completed_operations_.size() - start_idx);
    
    for (size_t i = start_idx; i < completed_operations_.size(); ++i) {
        result.push_back(std::shared_ptr<AsyncOperationTrace>(completed_operations_[i].get(), [](AsyncOperationTrace*){}));
    }
    
    return result;
}

std::vector<std::shared_ptr<AsyncOperationTrace>> AsyncTracer::getFailedOperations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<AsyncOperationTrace>> result;
    
    for (const auto& trace : completed_operations_) {
        if (trace->state == AsyncOperationState::FAILED) {
            result.push_back(std::shared_ptr<AsyncOperationTrace>(trace.get(), [](AsyncOperationTrace*){}));
        }
    }
    
    return result;
}

std::vector<AsyncDependency> AsyncTracer::getDependencies(const std::string& operation_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AsyncDependency> result;
    
    for (const auto& dep : dependencies_) {
        if (dep.parent_id == operation_id || dep.child_id == operation_id) {
            result.push_back(dep);
        }
    }
    
    return result;
}

std::string AsyncTracer::generateOperationReport(const std::string& operation_id) const {
    auto trace = getOperation(operation_id);
    if (!trace) {
        return "Operation not found: " + operation_id;
    }
    
    std::ostringstream report;
    report << "\n=== Async Operation Report ===\n";
    report << "Operation ID: " << trace->operation_id << "\n";
    report << "Name: " << trace->operation_name << "\n";
    report << "Context: " << trace->context << "\n";
    report << "State: " << to_string(trace->state) << "\n";
    report << "Duration: " << trace->getDuration().count() << "ms\n";
    report << "Execution Duration: " << trace->getExecutionDuration().count() << "ms\n";
    report << "Creator Thread: " << trace->creator_thread << "\n";
    report << "Executor Thread: " << trace->executor_thread << "\n";
    
    if (!trace->error_message.empty()) {
        report << "Error: " << trace->error_message << "\n";
    }
    
    if (!trace->metadata.empty()) {
        report << "\nMetadata:\n";
        for (const auto& pair : trace->metadata) {
            report << "  " << pair.first << ": " << pair.second << "\n";
        }
    }
    
    if (!trace->state_history.empty()) {
        report << "\nState History:\n";
        for (const auto& state : trace->state_history) {
            report << "  " << state << "\n";
        }
    }
    
    auto deps = getDependencies(operation_id);
    if (!deps.empty()) {
        report << "\nDependencies:\n";
        for (const auto& dep : deps) {
            if (dep.parent_id == operation_id) {
                report << "  Depends on: " << dep.child_id << " (" << dep.relationship_type << ")\n";
            } else {
                report << "  Required by: " << dep.parent_id << " (" << dep.relationship_type << ")\n";
            }
        }
    }
    
    return report.str();
}

std::string AsyncTracer::generateSummaryReport() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream report;
    report << "\n=== Async Operations Summary ===\n";
    report << "Active operations: " << active_operations_.size() << "\n";
    report << "Completed operations: " << completed_operations_.size() << "\n";
    report << "Total dependencies: " << dependencies_.size() << "\n";
    
    // Count by state
    std::unordered_map<AsyncOperationState, size_t> state_counts;
    for (const auto& trace : completed_operations_) {
        state_counts[trace->state]++;
    }
    
    if (!state_counts.empty()) {
        report << "\nCompleted Operations by State:\n";
        for (const auto& pair : state_counts) {
            report << "  " << to_string(pair.first) << ": " << pair.second << "\n";
        }
    }
    
    // Long running operations
    auto long_running = getLongRunningOperations(std::chrono::seconds(5));
    if (!long_running.empty()) {
        report << "\nLong Running Operations (>5s):\n";
        for (const auto& op_id : long_running) {
            auto trace = getOperation(op_id);
            if (trace) {
                report << "  " << op_id << " (" << trace->operation_name << "): " 
                       << trace->getDuration().count() << "ms\n";
            }
        }
    }
    
    return report.str();
}

void AsyncTracer::logOperationReport(const std::string& operation_id) const {
    LOG_INFO(generateOperationReport(operation_id));
}

void AsyncTracer::logSummaryReport() const {
    LOG_INFO(generateSummaryReport());
}

void AsyncTracer::logActiveOperations() const {
    auto active = getActiveOperations();
    if (active.empty()) {
        LOG_INFO("No active async operations");
        return;
    }
    
    std::ostringstream report;
    report << "\n=== Active Async Operations ===\n";
    for (const auto& trace : active) {
        report << trace->operation_id << " (" << trace->operation_name << "): " 
               << to_string(trace->state) << " - " << trace->getDuration().count() << "ms\n";
    }
    
    LOG_INFO(report.str());
}

void AsyncTracer::logFailedOperations() const {
    auto failed = getFailedOperations();
    if (failed.empty()) {
        LOG_INFO("No failed async operations");
        return;
    }
    
    std::ostringstream report;
    report << "\n=== Failed Async Operations ===\n";
    for (const auto& trace : failed) {
        report << trace->operation_id << " (" << trace->operation_name << "): " 
               << trace->error_message << "\n";
    }
    
    LOG_WARN(report.str());
}

void AsyncTracer::clearCompletedOperations() {
    std::lock_guard<std::mutex> lock(mutex_);
    completed_operations_.clear();
    LOG_INFO("Cleared completed async operations");
}

void AsyncTracer::clearAllOperations() {
    std::lock_guard<std::mutex> lock(mutex_);
    active_operations_.clear();
    completed_operations_.clear();
    dependencies_.clear();
    LOG_INFO("Cleared all async operations and dependencies");
}

size_t AsyncTracer::getActiveOperationCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_operations_.size();
}

size_t AsyncTracer::getCompletedOperationCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return completed_operations_.size();
}

std::vector<std::string> AsyncTracer::getLongRunningOperations(std::chrono::milliseconds threshold) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    
    for (const auto& pair : active_operations_) {
        if (pair.second->getDuration() > threshold) {
            result.push_back(pair.first);
        }
    }
    
    return result;
}

} // namespace AgenticFramework