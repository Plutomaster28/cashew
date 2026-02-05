#include "logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>

namespace cashew::utils {

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::init(const std::string& level, bool log_to_file) {
    std::vector<spdlog::sink_ptr> sinks;
    
    // Console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    sinks.push_back(console_sink);
    
    // File sink (optional)
    if (log_to_file) {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "cashew.log", 
            1024 * 1024 * 10,  // 10MB
            3                   // 3 rotating files
        );
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        sinks.push_back(file_sink);
    }
    
    // Create logger
    logger_ = std::make_shared<spdlog::logger>("cashew", sinks.begin(), sinks.end());
    
    // Set log level
    if (level == "trace") {
        logger_->set_level(spdlog::level::trace);
    } else if (level == "debug") {
        logger_->set_level(spdlog::level::debug);
    } else if (level == "info") {
        logger_->set_level(spdlog::level::info);
    } else if (level == "warn") {
        logger_->set_level(spdlog::level::warn);
    } else if (level == "error") {
        logger_->set_level(spdlog::level::err);
    } else if (level == "critical") {
        logger_->set_level(spdlog::level::critical);
    } else {
        logger_->set_level(spdlog::level::info);
    }
    
    // Flush on error or higher
    logger_->flush_on(spdlog::level::err);
    
    // Register as default logger
    spdlog::set_default_logger(logger_);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    if (!logger_) {
        init();
    }
    return logger_;
}

} // namespace cashew::utils
