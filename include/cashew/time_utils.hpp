#pragma once

#include "cashew/common.hpp"
#include <chrono>
#include <string>
#include <thread>

namespace cashew {
namespace time {

// Type aliases for convenience
using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::system_clock::duration;
using Seconds = std::chrono::seconds;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;

// Get current time
TimePoint now();

// Get current Unix timestamp (seconds since epoch)
uint64_t timestamp_seconds();

// Get current Unix timestamp (milliseconds since epoch)
uint64_t timestamp_milliseconds();

// Get current Unix timestamp (microseconds since epoch)
uint64_t timestamp_microseconds();

// Convert TimePoint to Unix timestamp (seconds)
uint64_t to_timestamp(const TimePoint& tp);

// Convert Unix timestamp to TimePoint
TimePoint from_timestamp(uint64_t timestamp_seconds);

// Convert TimePoint to string (ISO 8601 format)
std::string to_string(const TimePoint& tp);

// Parse ISO 8601 string to TimePoint
TimePoint from_string(const std::string& str);

// Duration utilities
template<typename Rep, typename Period>
inline uint64_t duration_to_seconds(const std::chrono::duration<Rep, Period>& duration) {
    return std::chrono::duration_cast<Seconds>(duration).count();
}

template<typename Rep, typename Period>
inline uint64_t duration_to_milliseconds(const std::chrono::duration<Rep, Period>& duration) {
    return std::chrono::duration_cast<Milliseconds>(duration).count();
}

// Epoch management (for Cashew's 10-minute epochs)
class EpochManager {
public:
    explicit EpochManager(uint32_t epoch_duration_seconds = constants::EPOCH_DURATION_SECONDS)
        : epoch_duration_(epoch_duration_seconds) {}
    
    // Get current epoch number (based on Unix timestamp)
    uint64_t current_epoch() const;
    
    // Get epoch number for a specific timestamp
    uint64_t epoch_for_timestamp(uint64_t timestamp) const;
    
    // Get start time of an epoch
    uint64_t epoch_start_time(uint64_t epoch) const;
    
    // Get end time of an epoch
    uint64_t epoch_end_time(uint64_t epoch) const;
    
    // Check if we're in a specific epoch
    bool is_in_epoch(uint64_t epoch) const;
    
    // Get time remaining in current epoch (seconds)
    uint64_t time_remaining_in_epoch() const;
    
    // Get time elapsed in current epoch (seconds)
    uint64_t time_elapsed_in_epoch() const;
    
    // Get epoch duration
    uint32_t epoch_duration() const { return epoch_duration_; }
    
private:
    uint32_t epoch_duration_; // in seconds
};

// Timer for measuring elapsed time
class Timer {
public:
    Timer() : start_(now()) {}
    
    // Reset the timer
    void reset() { start_ = now(); }
    
    // Get elapsed time in seconds
    double elapsed_seconds() const {
        auto duration = now() - start_;
        return std::chrono::duration<double>(duration).count();
    }
    
    // Get elapsed time in milliseconds
    uint64_t elapsed_milliseconds() const {
        return duration_to_milliseconds(now() - start_);
    }
    
    // Get elapsed time in microseconds
    uint64_t elapsed_microseconds() const {
        return std::chrono::duration_cast<Microseconds>(now() - start_).count();
    }
    
private:
    TimePoint start_;
};

// Timeout checker
class Timeout {
public:
    Timeout(uint32_t timeout_seconds) 
        : timeout_(Seconds(timeout_seconds))
        , start_(now()) {}
    
    // Check if timeout has expired
    bool expired() const {
        return (now() - start_) >= timeout_;
    }
    
    // Get time remaining (seconds), returns 0 if expired
    double remaining() const {
        auto elapsed = now() - start_;
        if (elapsed >= timeout_) return 0.0;
        return std::chrono::duration<double>(timeout_ - elapsed).count();
    }
    
    // Reset the timeout
    void reset() { start_ = now(); }
    
private:
    Duration timeout_;
    TimePoint start_;
};

// Rate limiter based on time windows
class RateLimiter {
public:
    RateLimiter(uint32_t max_operations, uint32_t window_seconds)
        : max_operations_(max_operations)
        , window_(Seconds(window_seconds)) {}
    
    // Check if operation is allowed
    bool allow();
    
    // Reset the rate limiter
    void reset();
    
    // Get number of operations in current window
    size_t count() const { return timestamps_.size(); }
    
private:
    uint32_t max_operations_;
    Duration window_;
    std::vector<TimePoint> timestamps_;
    
    void cleanup_old_timestamps();
};

// Sleep utilities
inline void sleep_seconds(uint32_t seconds) {
    std::this_thread::sleep_for(Seconds(seconds));
}

inline void sleep_milliseconds(uint32_t milliseconds) {
    std::this_thread::sleep_for(Milliseconds(milliseconds));
}

inline void sleep_microseconds(uint64_t microseconds) {
    std::this_thread::sleep_for(Microseconds(microseconds));
}

} // namespace time
} // namespace cashew
