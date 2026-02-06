#pragma once

#include "core/decay/decay.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

namespace cashew::decay {

/**
 * DecayRunner - Active enforcement engine that runs decay checks periodically
 */
class DecayRunner {
public:
    using DecayCallback = std::function<void(const std::vector<KeyDecayEvent>&, const std::vector<ThingDecayEvent>&)>;
    
    explicit DecayRunner(DecayScheduler& scheduler);
    ~DecayRunner();
    
    // Start/stop the runner
    void start(std::chrono::seconds interval = std::chrono::seconds(600)); // Default: 10 minutes
    void stop();
    bool is_running() const { return running_; }
    
    // Set callback for decay events
    void set_decay_callback(DecayCallback callback);
    
    // Manual trigger
    void trigger_check();
    
    // Statistics
    struct Stats {
        uint64_t total_checks;
        uint64_t last_check_timestamp;
        uint32_t keys_decayed_total;
        uint32_t things_decayed_total;
        uint64_t uptime_seconds;
    };
    
    Stats get_stats() const;
    
private:
    void run_loop();
    void perform_decay_check();
    
    DecayScheduler& scheduler_;
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> trigger_{false};
    DecayCallback decay_callback_;
    
    // Stats
    mutable std::mutex stats_mutex_;
    Stats stats_;
    std::chrono::system_clock::time_point start_time_;
};

} // namespace cashew::decay
