#include "config.hpp"
#include <fstream>
#include <stdexcept>

namespace cashew::utils {

Config Config::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path);
    }
    
    Config config;
    try {
        file >> config.data_;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Failed to parse config file: " + std::string(e.what()));
    }
    
    return config;
}

Config Config::load_from_json(const std::string& json_str) {
    Config config;
    try {
        config.data_ = json::parse(json_str);
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Failed to parse JSON: " + std::string(e.what()));
    }
    return config;
}

void Config::save_to_file(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }
    
    file << data_.dump(2); // Pretty print with 2-space indent
}

} // namespace cashew::utils
