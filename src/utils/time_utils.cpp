#include "cashew/time_utils.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>

#ifdef _WIN32
// Windows doesn't have timegm, provide a replacement
static time_t timegm_portable(struct tm* tm) {
    time_t ret;
    char* tz = getenv("TZ");
    _putenv_s("TZ", "UTC");
    _tzset();
    ret = mktime(tm);
    if (tz) {
        _putenv_s("TZ", tz);
    } else {
        _putenv_s("TZ", "");
    }
    _tzset();
    return ret;
}
#define timegm timegm_portable
#endif

namespace cashew {
namespace time {

TimePoint now() {
    return Clock::now();
}

uint64_t timestamp_seconds() {
    return std::chrono::duration_cast<Seconds>(
        Clock::now().time_since_epoch()
    ).count();
}

uint64_t timestamp_milliseconds() {
    return std::chrono::duration_cast<Milliseconds>(
        Clock::now().time_since_epoch()
    ).count();
}

uint64_t timestamp_microseconds() {
    return std::chrono::duration_cast<Microseconds>(
        Clock::now().time_since_epoch()
    ).count();
}

uint64_t to_timestamp(const TimePoint& tp) {
    return std::chrono::duration_cast<Seconds>(
        tp.time_since_epoch()
    ).count();
}

TimePoint from_timestamp(uint64_t timestamp_seconds) {
    return TimePoint(Seconds(timestamp_seconds));
}

std::string to_string(const TimePoint& tp) {
    auto time_t_val = Clock::to_time_t(tp);
    std::tm tm_val;
    
#ifdef CASHEW_PLATFORM_WINDOWS
    gmtime_s(&tm_val, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm_val);
#endif
    
    auto ms = std::chrono::duration_cast<Milliseconds>(
        tp.time_since_epoch()
    ).count() % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms << 'Z';
    return oss.str();
}

TimePoint from_string(const std::string& str) {
    // Simple ISO 8601 parser (YYYY-MM-DDTHH:MM:SS.sssZ)
    std::tm tm_val = {};
    std::istringstream iss(str);
    
    char delimiter;
    iss >> std::get_time(&tm_val, "%Y-%m-%dT%H:%M:%S");
    
    if (iss.fail()) {
        throw std::runtime_error("Failed to parse time string: " + str);
    }
    
    // Parse milliseconds if present
    int ms = 0;
    if (iss.peek() == '.') {
        iss >> delimiter; // consume '.'
        iss >> ms;
    }
    
    auto time_t_val = timegm(&tm_val);
    auto tp = Clock::from_time_t(time_t_val);
    tp += Milliseconds(ms);
    
    return tp;
}

// EpochManager implementation
uint64_t EpochManager::current_epoch() const {
    return epoch_for_timestamp(timestamp_seconds());
}

uint64_t EpochManager::epoch_for_timestamp(uint64_t timestamp) const {
    return timestamp / epoch_duration_;
}

uint64_t EpochManager::epoch_start_time(uint64_t epoch) const {
    return epoch * epoch_duration_;
}

uint64_t EpochManager::epoch_end_time(uint64_t epoch) const {
    return (epoch + 1) * epoch_duration_;
}

bool EpochManager::is_in_epoch(uint64_t epoch) const {
    return current_epoch() == epoch;
}

uint64_t EpochManager::time_remaining_in_epoch() const {
    auto current = current_epoch();
    auto end = epoch_end_time(current);
    auto now_ts = timestamp_seconds();
    return (now_ts < end) ? (end - now_ts) : 0;
}

uint64_t EpochManager::time_elapsed_in_epoch() const {
    auto current = current_epoch();
    auto start = epoch_start_time(current);
    auto now_ts = timestamp_seconds();
    return (now_ts >= start) ? (now_ts - start) : 0;
}

// RateLimiter implementation
bool RateLimiter::allow() {
    cleanup_old_timestamps();
    
    if (timestamps_.size() < max_operations_) {
        timestamps_.push_back(now());
        return true;
    }
    
    return false;
}

void RateLimiter::reset() {
    timestamps_.clear();
}

void RateLimiter::cleanup_old_timestamps() {
    auto cutoff = now() - window_;
    timestamps_.erase(
        std::remove_if(
            timestamps_.begin(),
            timestamps_.end(),
            [cutoff](const TimePoint& tp) { return tp < cutoff; }
        ),
        timestamps_.end()
    );
}

} // namespace time
} // namespace cashew
