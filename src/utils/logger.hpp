#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

namespace cashew::utils {

/**
 * Logging system wrapper around spdlog
 */
class Logger {
public:
    /**
     * Initialize the logging system
     * @param level Log level (trace, debug, info, warn, error, critical)
     * @param log_to_file Whether to log to file in addition to console
     */
    static void init(const std::string& level = "info", bool log_to_file = false);
    
    /**
     * Get the logger instance
     */
    static std::shared_ptr<spdlog::logger> get();
    
private:
    static std::shared_ptr<spdlog::logger> logger_;
};

} // namespace cashew::utils

// Convenience macros
#define CASHEW_LOG_TRACE(...)    cashew::utils::Logger::get()->trace(__VA_ARGS__)
#define CASHEW_LOG_DEBUG(...)    cashew::utils::Logger::get()->debug(__VA_ARGS__)
#define CASHEW_LOG_INFO(...)     cashew::utils::Logger::get()->info(__VA_ARGS__)
#define CASHEW_LOG_WARN(...)     cashew::utils::Logger::get()->warn(__VA_ARGS__)
#define CASHEW_LOG_ERROR(...)    cashew::utils::Logger::get()->error(__VA_ARGS__)
#define CASHEW_LOG_CRITICAL(...) cashew::utils::Logger::get()->critical(__VA_ARGS__)
