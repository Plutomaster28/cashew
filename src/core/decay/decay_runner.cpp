#include "core/decay/decay_runner.hpp"
#include "utils/logger.hpp"

namespace cashew::decay {

DecayRunner::DecayRunner(DecayScheduler& scheduler)
    : scheduler_(scheduler)
{
    stats_ = {};
}

DecayRunner::~DecayRunner() {
    stop();
}

void DecayRunner::start(std::chrono::seconds interval) {
    if (running_) {
        CASHEW_LOG_WARN("DecayRunner already running");
        return;
    }
    
    running_ = true;
    start_time_ = std::chrono::system_clock::now();
    
    worker_thread_ = std::thread([this, interval]() {
        CASHEW_LOG_INFO("DecayRunner started with {}-second interval", interval.count());
        
        while (running_) {
            // Wait for interval or manual trigger
            auto wait_start = std::chrono::steady_clock::now();
            
            while (running_) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - wait_start
                );
                
                if (trigger_.load() || elapsed >= interval) {
                    break;
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            if (!running_) break;
            
            // Perform decay check
            if (trigger_.load()) {
                CASHEW_LOG_DEBUG("Manual decay check triggered");
                trigger_ = false;
            }
            
            perform_decay_check();
        }
        
        CASHEW_LOG_INFO("DecayRunner stopped");
    });
}

void DecayRunner::stop() {
    if (!running_) {
        return;
    }
    
    CASHEW_LOG_INFO("Stopping DecayRunner...");
    running_ = false;
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void DecayRunner::set_decay_callback(DecayCallback callback) {
    decay_callback_ = std::move(callback);
}

void DecayRunner::trigger_check() {
    if (!running_) {
        CASHEW_LOG_WARN("Cannot trigger check: DecayRunner not running");
        return;
    }
    
    trigger_ = true;
}

void DecayRunner::perform_decay_check() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current_epoch = static_cast<uint64_t>(now_time_t) / (10 * 60);  // 10-minute epochs
    
    CASHEW_LOG_DEBUG("Performing decay check for epoch {}", current_epoch);
    
    try {
        // Check for decayable keys
        auto key_decays = scheduler_.check_key_decay(current_epoch);
        
        // Check for decayable Things
        auto thing_decays = scheduler_.check_thing_decay();
        
        // Apply decays
        for (const auto& decay : key_decays) {
            scheduler_.apply_key_decay(decay);
        }
        
        for (const auto& decay : thing_decays) {
            scheduler_.apply_thing_decay(decay);
        }
        
        // Update stats
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_checks++;
            stats_.last_check_timestamp = static_cast<uint64_t>(now_time_t);
            stats_.keys_decayed_total += static_cast<uint32_t>(key_decays.size());
            stats_.things_decayed_total += static_cast<uint32_t>(thing_decays.size());
        }
        
        // Invoke callback if set
        if (decay_callback_ && (!key_decays.empty() || !thing_decays.empty())) {
            decay_callback_(key_decays, thing_decays);
        }
        
        if (!key_decays.empty() || !thing_decays.empty()) {
            CASHEW_LOG_INFO("Decay check complete: {} keys, {} Things removed",
                           key_decays.size(), thing_decays.size());
        }
        
    } catch (const std::exception& e) {
        CASHEW_LOG_ERROR("Error during decay check: {}", e.what());
    }
}

DecayRunner::Stats DecayRunner::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto stats = stats_;
    
    if (running_) {
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        stats.uptime_seconds = uptime.count();
    }
    
    return stats;
}

} // namespace cashew::decay
