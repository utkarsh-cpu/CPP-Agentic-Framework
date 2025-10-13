#ifndef TIMEOUTPOLICY_H
#define TIMEOUTPOLICY_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

/**
 * @brief Defines timeout behavior for different types of operations
 */
class TimeoutPolicy {
public:
    /**
     * @brief Predefined operation types with default timeouts
     */
    enum class OperationType {
        FAST_OPERATION,      // Quick operations (1 second)
        NORMAL_OPERATION,    // Standard operations (30 seconds)
        SLOW_OPERATION,      // Long-running operations (5 minutes)
        BATCH_OPERATION,     // Batch processing (15 minutes)
        NETWORK_OPERATION,   // Network calls (10 seconds)
        IO_OPERATION,        // File I/O operations (30 seconds)
        CUSTOM              // User-defined timeout
    };

private:
    std::chrono::milliseconds timeout_;
    bool enable_cancellation_;
    std::chrono::milliseconds cleanup_timeout_;

public:
    /**
     * @brief Construct a timeout policy with specified parameters
     * @param timeout Maximum time to wait for operation completion
     * @param enable_cancellation Whether to support cancellation
     * @param cleanup_timeout Maximum time to wait for cleanup operations
     */
    explicit TimeoutPolicy(
        std::chrono::milliseconds timeout = std::chrono::seconds(30),
        bool enable_cancellation = true,
        std::chrono::milliseconds cleanup_timeout = std::chrono::seconds(5)
    ) : timeout_(timeout)
      , enable_cancellation_(enable_cancellation)
      , cleanup_timeout_(cleanup_timeout) {}

    /**
     * @brief Create a timeout policy for a specific operation type
     * @param type The type of operation
     * @return A timeout policy configured for the operation type
     */
    static TimeoutPolicy for_operation(OperationType type) {
        switch (type) {
            case OperationType::FAST_OPERATION:
                return TimeoutPolicy(std::chrono::seconds(1), true, std::chrono::milliseconds(500));
            case OperationType::NORMAL_OPERATION:
                return TimeoutPolicy(std::chrono::seconds(30), true, std::chrono::seconds(5));
            case OperationType::SLOW_OPERATION:
                return TimeoutPolicy(std::chrono::minutes(5), true, std::chrono::seconds(10));
            case OperationType::BATCH_OPERATION:
                return TimeoutPolicy(std::chrono::minutes(15), true, std::chrono::seconds(30));
            case OperationType::NETWORK_OPERATION:
                return TimeoutPolicy(std::chrono::seconds(10), true, std::chrono::seconds(2));
            case OperationType::IO_OPERATION:
                return TimeoutPolicy(std::chrono::seconds(30), true, std::chrono::seconds(5));
            case OperationType::CUSTOM:
            default:
                return TimeoutPolicy();
        }
    }

    /**
     * @brief Get the timeout duration
     * @return The timeout duration
     */
    [[nodiscard]] std::chrono::milliseconds timeout() const noexcept {
        return timeout_;
    }

    /**
     * @brief Check if cancellation is enabled
     * @return true if cancellation is enabled, false otherwise
     */
    [[nodiscard]] bool cancellation_enabled() const noexcept {
        return enable_cancellation_;
    }

    /**
     * @brief Get the cleanup timeout duration
     * @return The cleanup timeout duration
     */
    [[nodiscard]] std::chrono::milliseconds cleanup_timeout() const noexcept {
        return cleanup_timeout_;
    }

    /**
     * @brief Set the timeout duration
     * @param timeout New timeout duration
     * @return Reference to this policy for chaining
     */
    TimeoutPolicy& set_timeout(std::chrono::milliseconds timeout) {
        timeout_ = timeout;
        return *this;
    }

    /**
     * @brief Enable or disable cancellation
     * @param enable Whether to enable cancellation
     * @return Reference to this policy for chaining
     */
    TimeoutPolicy& set_cancellation_enabled(bool enable) {
        enable_cancellation_ = enable;
        return *this;
    }

    /**
     * @brief Set the cleanup timeout duration
     * @param cleanup_timeout New cleanup timeout duration
     * @return Reference to this policy for chaining
     */
    TimeoutPolicy& set_cleanup_timeout(std::chrono::milliseconds cleanup_timeout) {
        cleanup_timeout_ = cleanup_timeout;
        return *this;
    }
};

/**
 * @brief Registry for managing timeout policies by name
 */
class TimeoutPolicyRegistry {
private:
    std::unordered_map<std::string, TimeoutPolicy> policies_;
    mutable std::mutex mutex_;

public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton registry
     */
    static TimeoutPolicyRegistry& instance() {
        static TimeoutPolicyRegistry instance;
        return instance;
    }

    /**
     * @brief Register a timeout policy with a name
     * @param name Name of the policy
     * @param policy The timeout policy
     */
    void register_policy(const std::string& name, const TimeoutPolicy& policy) {
        std::lock_guard<std::mutex> lock(mutex_);
        policies_[name] = policy;
    }

    /**
     * @brief Get a timeout policy by name
     * @param name Name of the policy
     * @return The timeout policy, or default policy if not found
     */
    [[nodiscard]] TimeoutPolicy get_policy(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = policies_.find(name);
        return it != policies_.end() ? it->second : TimeoutPolicy();
    }

    /**
     * @brief Check if a policy exists
     * @param name Name of the policy
     * @return true if the policy exists, false otherwise
     */
    [[nodiscard]] bool has_policy(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return policies_.find(name) != policies_.end();
    }

    /**
     * @brief Remove a policy
     * @param name Name of the policy to remove
     * @return true if the policy was removed, false if it didn't exist
     */
    bool remove_policy(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return policies_.erase(name) > 0;
    }

private:
    TimeoutPolicyRegistry() {
        // Register default policies
        register_policy("fast", TimeoutPolicy::for_operation(TimeoutPolicy::OperationType::FAST_OPERATION));
        register_policy("normal", TimeoutPolicy::for_operation(TimeoutPolicy::OperationType::NORMAL_OPERATION));
        register_policy("slow", TimeoutPolicy::for_operation(TimeoutPolicy::OperationType::SLOW_OPERATION));
        register_policy("batch", TimeoutPolicy::for_operation(TimeoutPolicy::OperationType::BATCH_OPERATION));
        register_policy("network", TimeoutPolicy::for_operation(TimeoutPolicy::OperationType::NETWORK_OPERATION));
        register_policy("io", TimeoutPolicy::for_operation(TimeoutPolicy::OperationType::IO_OPERATION));
    }
};

#endif // TIMEOUTPOLICY_H