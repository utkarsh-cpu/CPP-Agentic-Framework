#ifndef CANCELLATIONTOKEN_H
#define CANCELLATIONTOKEN_H

#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include <chrono>

/**
 * @brief A thread-safe cancellation token that can be used to signal cancellation
 * to long-running operations and coordinate cleanup.
 */
class CancellationToken {
private:
    std::shared_ptr<std::atomic<bool>> is_cancelled_;
    std::shared_ptr<std::vector<std::function<void()>>> callbacks_;
    std::shared_ptr<std::mutex> callbacks_mutex_;

public:
    /**
     * @brief Construct a new cancellation token
     */
    CancellationToken() 
        : is_cancelled_(std::make_shared<std::atomic<bool>>(false))
        , callbacks_(std::make_shared<std::vector<std::function<void()>>>())
        , callbacks_mutex_(std::make_shared<std::mutex>()) {}

    /**
     * @brief Check if cancellation has been requested
     * @return true if cancellation was requested, false otherwise
     */
    [[nodiscard]] bool is_cancellation_requested() const noexcept {
        return is_cancelled_->load(std::memory_order_acquire);
    }

    /**
     * @brief Throw if cancellation has been requested
     * @throws std::runtime_error if cancellation was requested
     */
    void throw_if_cancellation_requested() const {
        if (is_cancellation_requested()) {
            throw std::runtime_error("Operation was cancelled");
        }
    }

    /**
     * @brief Register a callback to be called when cancellation is requested
     * @param callback Function to call on cancellation
     */
    void register_callback(std::function<void()> callback) {
        std::lock_guard<std::mutex> lock(*callbacks_mutex_);
        if (is_cancellation_requested()) {
            // If already cancelled, execute callback immediately
            callback();
        } else {
            callbacks_->emplace_back(std::move(callback));
        }
    }

    /**
     * @brief Request cancellation and execute all registered callbacks
     */
    void cancel() {
        bool expected = false;
        if (is_cancelled_->compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            // We were the first to cancel, execute callbacks
            std::lock_guard<std::mutex> lock(*callbacks_mutex_);
            for (const auto& callback : *callbacks_) {
                try {
                    callback();
                } catch (...) {
                    // Suppress exceptions in callbacks to prevent cascading failures
                }
            }
            callbacks_->clear();
        }
    }

    /**
     * @brief Create a token that is already cancelled
     * @return A cancelled cancellation token
     */
    static CancellationToken cancelled() {
        CancellationToken token;
        token.cancel();
        return token;
    }

    /**
     * @brief Create a token that will be cancelled after a timeout
     * @param timeout Duration after which to cancel
     * @return A cancellation token that will be cancelled after timeout
     */
    static CancellationToken with_timeout(std::chrono::milliseconds timeout);
};

/**
 * @brief A source for creating and managing cancellation tokens
 */
class CancellationTokenSource {
private:
    CancellationToken token_;

public:
    /**
     * @brief Get the cancellation token managed by this source
     * @return The cancellation token
     */
    [[nodiscard]] const CancellationToken& token() const noexcept {
        return token_;
    }

    /**
     * @brief Request cancellation
     */
    void cancel() {
        token_.cancel();
    }

    /**
     * @brief Check if cancellation has been requested
     * @return true if cancellation was requested, false otherwise
     */
    [[nodiscard]] bool is_cancellation_requested() const noexcept {
        return token_.is_cancellation_requested();
    }
};

#endif // CANCELLATIONTOKEN_H