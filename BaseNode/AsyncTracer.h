#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <future>
#include <functional>
#include "Logger.h"
#include "PerformanceProfiler.h"

namespace AgenticFramework {

// Async operation states
enum class AsyncOperationState {
    CREATED,
    STARTED,
    RUNNING,
    WAITING,
    COMPLETED,
    FAILED,
    CANCELLED,
    TIMEOUT
};

inline const char* to_string(AsyncOperationState state) {
    switch (state) {
        case AsyncOperationState::CREATED: return "CREATED";
        case AsyncOperationState::STARTED: return "STARTED";
        case AsyncOperationState::RUNNING: return "RUNNING";
        case AsyncOperationState::WAITING: return "WAITING";
        case AsyncOperationState::COMPLETED: return "COMPLETED";
        case AsyncOperationState::FAILED: return "FAILED";
        case AsyncOperationState::CANCELLED: return "CANCELLED";
        case AsyncOperationState::TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}

// Async operation trace information
struct AsyncOperationTrace {
    std::string operation_id;
    std::string operation_name;
    std::string context;
    AsyncOperationState state;
    std::chrono::system_clock::time_point created_time;
    std::chrono::system_clock::time_point started_time;
    std::chrono::system_clock::time_point completed_time;
    std::thread::id creator_thread;
    std::thread::id executor_thread;
    std::string error_message;
    std::vector<std::string> state_history;
    std::unordered_map<std::string, std::string> metadata;
    
    AsyncOperationTrace(std::string id, std::string name, std::string ctx = "")
        : operation_id(std::move(id))
        , operation_name(std::move(name))
        , context(std::move(ctx))
        , state(AsyncOperationState::CREATED)
        , created_time(std::chrono::system_clock::now())
        , creator_thread(std::this_thread::get_id()) {
        state_history.push_back("CREATED");
    }
    
    void updateState(AsyncOperationState new_state, const std::string& message = "") {
        state = new_state;
        state_history.push_back(std::string(to_string(new_state)) + 
                               (message.empty() ? "" : ": " + message));
        
        if (new_state == AsyncOperationState::STARTED) {
            started_time = std::chrono::system_clock::now();
            executor_thread = std::this_thread::get_id();
        } else if (new_state == AsyncOperationState::COMPLETED || 
                   new_state == AsyncOperationState::FAILED ||
                   new_state == AsyncOperationState::CANCELLED ||
                   new_state == AsyncOperationState::TIMEOUT) {
            completed_time = std::chrono::system_clock::now();
        }
        
        if (new_state == AsyncOperationState::FAILED && !message.empty()) {
            error_message = message;
        }
    }
    
    void addMetadata(const std::string& key, const std::string& value) {
        metadata[key] = value;
    }
    
    std::chrono::milliseconds getDuration() const {
        auto end_time = (completed_time.time_since_epoch().count() > 0) ? 
                       completed_time : std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - created_time);
    }
    
    std::chrono::milliseconds getExecutionDuration() const {
        if (started_time.time_since_epoch().count() == 0) {
            return std::chrono::milliseconds(0);
        }
        auto end_time = (completed_time.time_since_epoch().count() > 0) ? 
                       completed_time : std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - started_time);
    }
    
    bool isCompleted() const {
        return state == AsyncOperationState::COMPLETED ||
               state == AsyncOperationState::FAILED ||
               state == AsyncOperationState::CANCELLED ||
               state == AsyncOperationState::TIMEOUT;
    }
};

// RAII async operation tracker
class ScopedAsyncTracker {
private:
    std::string operation_id_;
    bool auto_complete_;
    
public:
    ScopedAsyncTracker(const std::string& operation_name, const std::string& context = "", 
                      bool auto_complete = true);
    ~ScopedAsyncTracker();
    
    void updateState(AsyncOperationState state, const std::string& message = "");
    void addMetadata(const std::string& key, const std::string& value);
    void setError(const std::string& error_message);
    
    const std::string& getOperationId() const { return operation_id_; }
};

// Async operation dependency tracking
struct AsyncDependency {
    std::string parent_id;
    std::string child_id;
    std::string relationship_type;
    std::chrono::system_clock::time_point created_time;
    
    AsyncDependency(std::string parent, std::string child, std::string type = "depends_on")
        : parent_id(std::move(parent))
        , child_id(std::move(child))
        , relationship_type(std::move(type))
        , created_time(std::chrono::system_clock::now()) {}
};

// Main async tracer
class AsyncTracer {
private:
    std::unordered_map<std::string, std::unique_ptr<AsyncOperationTrace>> active_operations_;
    std::vector<std::unique_ptr<AsyncOperationTrace>> completed_operations_;
    std::vector<AsyncDependency> dependencies_;
    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{true};
    std::atomic<size_t> max_completed_operations_{1000};
    std::atomic<size_t> operation_counter_{0};
    
    static std::unique_ptr<AsyncTracer> instance_;
    static std::once_flag init_flag_;
    
    AsyncTracer() = default;
    
public:
    static AsyncTracer& getInstance();
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setMaxCompletedOperations(size_t max_operations);
    
    // Operation tracking
    std::string startOperation(const std::string& operation_name, const std::string& context = "");
    void updateOperationState(const std::string& operation_id, AsyncOperationState state, 
                             const std::string& message = "");
    void addOperationMetadata(const std::string& operation_id, const std::string& key, 
                             const std::string& value);
    void completeOperation(const std::string& operation_id);
    void failOperation(const std::string& operation_id, const std::string& error_message);
    void cancelOperation(const std::string& operation_id);
    void timeoutOperation(const std::string& operation_id);
    
    // Dependency tracking
    void addDependency(const std::string& parent_id, const std::string& child_id, 
                      const std::string& relationship_type = "depends_on");
    
    // Query operations
    std::shared_ptr<AsyncOperationTrace> getOperation(const std::string& operation_id) const;
    std::vector<std::shared_ptr<AsyncOperationTrace>> getActiveOperations() const;
    std::vector<std::shared_ptr<AsyncOperationTrace>> getCompletedOperations(size_t count = 100) const;
    std::vector<std::shared_ptr<AsyncOperationTrace>> getFailedOperations() const;
    std::vector<AsyncDependency> getDependencies(const std::string& operation_id) const;
    
    // Analysis and reporting
    std::string generateOperationReport(const std::string& operation_id) const;
    std::string generateSummaryReport() const;
    std::string generateDependencyGraph() const;
    
    void logOperationReport(const std::string& operation_id) const;
    void logSummaryReport() const;
    void logActiveOperations() const;
    void logFailedOperations() const;
    
    // Cleanup
    void clearCompletedOperations();
    void clearAllOperations();
    
    // Utility methods
    size_t getActiveOperationCount() const;
    size_t getCompletedOperationCount() const;
    std::vector<std::string> getLongRunningOperations(std::chrono::milliseconds threshold) const;
    
private:
    std::string generateOperationId();
    void moveToCompleted(const std::string& operation_id);
};

// Template wrapper for async operations with automatic tracing
template<typename F>
class TrackedAsyncOperation {
private:
    std::string operation_name_;
    std::string context_;
    F operation_;
    
public:
    TrackedAsyncOperation(std::string name, std::string context, F&& op)
        : operation_name_(std::move(name))
        , context_(std::move(context))
        , operation_(std::forward<F>(op)) {}
    
    template<typename... Args>
    auto operator()(Args&&... args) -> std::future<decltype(operation_(std::forward<Args>(args)...))> {
        return std::async(std::launch::async, [this, args...]() {
            ScopedAsyncTracker tracker(operation_name_, context_);
            
            try {
                tracker.updateState(AsyncOperationState::STARTED);
                tracker.updateState(AsyncOperationState::RUNNING);
                
                auto result = operation_(args...);
                
                tracker.updateState(AsyncOperationState::COMPLETED);
                return result;
            } catch (const std::exception& e) {
                tracker.setError(e.what());
                throw;
            } catch (...) {
                tracker.setError("Unknown exception");
                throw;
            }
        });
    }
};

// Helper function to create tracked async operations
template<typename F>
TrackedAsyncOperation<F> make_tracked_async(const std::string& name, const std::string& context, F&& operation) {
    return TrackedAsyncOperation<F>(name, context, std::forward<F>(operation));
}

} // namespace AgenticFramework

// Convenience macros for async tracing
#define TRACE_ASYNC_OPERATION(name) \
    AgenticFramework::ScopedAsyncTracker _async_tracker(name, __FUNCTION__)

#define TRACE_ASYNC_OPERATION_CONTEXT(name, context) \
    AgenticFramework::ScopedAsyncTracker _async_tracker(name, context)

#define UPDATE_ASYNC_STATE(state, message) \
    _async_tracker.updateState(state, message)

#define ADD_ASYNC_METADATA(key, value) \
    _async_tracker.addMetadata(key, value)

#define SET_ASYNC_ERROR(error) \
    _async_tracker.setError(error)