#include "CancellationToken.h"
#include <thread>
#include <future>

CancellationToken CancellationToken::with_timeout(std::chrono::milliseconds timeout) {
    CancellationToken token;
    
    // Create a detached thread that will cancel the token after the timeout
    std::thread timeout_thread([token_weak = std::weak_ptr<std::atomic<bool>>(token.is_cancelled_), timeout]() mutable {
        std::this_thread::sleep_for(timeout);
        
        // Check if the token still exists (hasn't been destroyed)
        if (auto token_ptr = token_weak.lock()) {
            // Cancel the token if it hasn't been cancelled already
            bool expected = false;
            token_ptr->compare_exchange_strong(expected, true, std::memory_order_acq_rel);
        }
    });
    
    timeout_thread.detach();
    return token;
}