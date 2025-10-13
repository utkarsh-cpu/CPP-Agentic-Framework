#include "MemoryTracker.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace AgenticFramework {

std::unique_ptr<MemoryTracker> MemoryTracker::instance_;
std::once_flag MemoryTracker::init_flag_;

// MemoryPool implementation
void MemoryPool::recordAllocation(void* ptr, size_t size, std::string_view file, 
                                 int line, std::string_view function) {
    if (!ptr) return;
    
    total_allocated_.fetch_add(size);
    current_usage_.fetch_add(size);
    allocation_count_.fetch_add(1);
    
    // Update peak usage
    size_t current = current_usage_.load();
    size_t peak = peak_usage_.load();
    while (current > peak && !peak_usage_.compare_exchange_weak(peak, current)) {
        // Retry if another thread updated peak_usage
    }
    
    if (track_allocations_.load()) {
        std::lock_guard<std::mutex> lock(mutex_);
        allocations_.emplace(ptr, AllocationInfo(ptr, size, std::string{file}, line, std::string{function}));
    }
}

void MemoryPool::recordDeallocation(void* ptr) {
    if (!ptr) return;
    
    size_t size = 0;
    
    if (track_allocations_.load()) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = allocations_.find(ptr);
        if (it != allocations_.end()) {
            size = it->second.size;
            allocations_.erase(it);
        }
    }
    
    if (size > 0) {
        total_deallocated_.fetch_add(size);
        current_usage_.fetch_sub(size);
    }
    deallocation_count_.fetch_add(1);
}

std::vector<AllocationInfo> MemoryPool::getActiveAllocations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AllocationInfo> result;
    result.reserve(allocations_.size());
    
    for (const auto& pair : allocations_) {
        result.push_back(pair.second);
    }
    
    // Sort by size descending
    std::sort(result.begin(), result.end(), 
              [](const AllocationInfo& a, const AllocationInfo& b) {
                  return a.size > b.size;
              });
    
    return result;
}

std::string MemoryPool::generateLeakReport() const {
    std::ostringstream report;
    
    auto leaks = getActiveAllocations();
    if (leaks.empty()) {
        report << "No memory leaks detected in pool '" << name_ << "'\n";
        return report.str();
    }
    
    report << "\n=== Memory Leak Report for Pool '" << name_ << "' ===\n";
    report << "Total leaked allocations: " << leaks.size() << "\n";
    auto stats = getStats();
    report << "Total leaked bytes: " << stats.getLeakedBytes() << "\n\n";
    
    report << std::left << std::setw(12) << "Size"
           << std::setw(20) << "Address"
           << std::setw(25) << "File"
           << std::setw(8) << "Line"
           << std::setw(20) << "Function" << "\n";
    report << std::string(85, '-') << "\n";
    
    for (const auto& leak : leaks) {
        std::string filename = leak.file;
        size_t pos = filename.find_last_of("/\\");
        if (pos != std::string::npos) {
            filename = filename.substr(pos + 1);
        }
        
        report << std::left << std::setw(12) << leak.size
               << std::setw(20) << leak.ptr
               << std::setw(25) << filename
               << std::setw(8) << leak.line
               << std::setw(20) << leak.function << "\n";
    }
    
    return report.str();
}

void MemoryPool::logLeakReport() const {
    auto report = generateLeakReport();
    if (getActiveAllocations().empty()) {
        LOG_INFO(report);
    } else {
        LOG_WARN(report);
    }
}

// MemoryTracker implementation
MemoryTracker& MemoryTracker::getInstance() {
    std::call_once(init_flag_, []() {
        instance_ = std::unique_ptr<MemoryTracker>(new MemoryTracker());
    });
    return *instance_;
}

void MemoryTracker::setEnabled(bool enabled) noexcept {
    enabled_.store(enabled);
    if (enabled) {
        LOG_INFO("Memory tracking enabled");
    } else {
        LOG_INFO("Memory tracking disabled");
    }
}

bool MemoryTracker::isEnabled() const noexcept {
    return enabled_.load();
}

MemoryPool& MemoryTracker::getPool(std::string_view name) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string name_str{name};
    auto it = pools_.find(name_str);
    if (it == pools_.end()) {
        auto pool = std::make_unique<MemoryPool>(name_str);
        auto& pool_ref = *pool;
        pools_[name_str] = std::move(pool);
        return pool_ref;
    }
    return *it->second;
}

void MemoryTracker::removePool(std::string_view name) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string name_str{name};
    auto it = pools_.find(name_str);
    if (it != pools_.end()) {
        // Log any remaining leaks before removing
        it->second->logLeakReport();
        pools_.erase(it);
    }
}

void MemoryTracker::recordGlobalAllocation(size_t size) noexcept {
    global_total_allocated_.fetch_add(size);
    global_current_usage_.fetch_add(size);
    global_allocation_count_.fetch_add(1);
    
    // Update peak usage
    size_t current = global_current_usage_.load();
    size_t peak = global_peak_usage_.load();
    while (current > peak && !global_peak_usage_.compare_exchange_weak(peak, current)) {
        // Retry if another thread updated peak_usage
    }
}

void MemoryTracker::recordGlobalDeallocation(size_t size) noexcept {
    global_total_deallocated_.fetch_add(size);
    global_current_usage_.fetch_sub(size);
    global_deallocation_count_.fetch_add(1);
}

std::vector<std::string> MemoryTracker::getPoolNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(pools_.size());
    
    for (const auto& pair : pools_) {
        names.push_back(pair.first);
    }
    
    return names;
}

std::string MemoryTracker::generateGlobalReport() const {
    std::ostringstream report;
    
    report << "\n=== Global Memory Usage Report ===\n";
    auto global_stats = getGlobalStats();
    report << "Current usage: " << global_stats.getCurrentUsage() << " bytes\n";
    report << "Peak usage: " << global_stats.getPeakUsage() << " bytes\n";
    report << "Total allocated: " << global_stats.getTotalAllocated() << " bytes\n";
    report << "Total deallocated: " << global_stats.getTotalDeallocated() << " bytes\n";
    report << "Allocation count: " << global_stats.getAllocationCount() << "\n";
    report << "Deallocation count: " << global_stats.getDeallocationCount() << "\n";
    report << "Leaked bytes: " << global_stats.getLeakedBytes() << "\n";
    report << "Leaked allocations: " << global_stats.getLeakedAllocations() << "\n";
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pools_.empty()) {
        report << "\n=== Memory Pool Summary ===\n";
        report << std::left << std::setw(20) << "Pool Name"
               << std::setw(15) << "Current (bytes)"
               << std::setw(15) << "Peak (bytes)"
               << std::setw(12) << "Allocations"
               << std::setw(12) << "Leaks" << "\n";
        report << std::string(74, '-') << "\n";
        
        for (const auto& pair : pools_) {
            const auto& stats = pair.second->getStats();
            report << std::left << std::setw(20) << pair.first
                   << std::setw(15) << stats.getCurrentUsage()
                   << std::setw(15) << stats.getPeakUsage()
                   << std::setw(12) << stats.getAllocationCount()
                   << std::setw(12) << stats.getLeakedAllocations() << "\n";
        }
    }
    
    return report.str();
}

void MemoryTracker::logGlobalReport() const {
    LOG_INFO(generateGlobalReport());
}

bool MemoryTracker::hasMemoryLeaks() const {
    if (getGlobalStats().getLeakedBytes() > 0) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : pools_) {
        if (pair.second->getStats().getLeakedBytes() > 0) {
            return true;
        }
    }
    
    return false;
}

void MemoryTracker::logMemoryLeaks() const {
    if (!hasMemoryLeaks()) {
        LOG_INFO("No memory leaks detected");
        return;
    }
    
    LOG_WARN("Memory leaks detected!");
    logGlobalReport();
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : pools_) {
        if (pair.second->getStats().getLeakedBytes() > 0) {
            pair.second->logLeakReport();
        }
    }
}

} // namespace AgenticFramework