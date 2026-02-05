#pragma once

#include <string>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>

namespace cashew::utils {

using json = nlohmann::json;

/**
 * Configuration management system
 * Supports loading from JSON/TOML files
 */
class Config {
public:
    Config() = default;
    
    /**
     * Load configuration from JSON file
     */
    static Config load_from_file(const std::string& path);
    
    /**
     * Load configuration from JSON string
     */
    static Config load_from_json(const std::string& json_str);
    
    /**
     * Save configuration to file
     */
    void save_to_file(const std::string& path) const;
    
    /**
     * Get a value from config
     */
    template<typename T>
    std::optional<T> get(const std::string& key) const {
        try {
            if (data_.contains(key)) {
                return data_[key].get<T>();
            }
        } catch (...) {
        }
        return std::nullopt;
    }
    
    /**
     * Get a value with default
     */
    template<typename T>
    T get_or(const std::string& key, const T& default_value) const {
        auto value = get<T>(key);
        return value.value_or(default_value);
    }
    
    /**
     * Set a value
     */
    template<typename T>
    void set(const std::string& key, const T& value) {
        data_[key] = value;
    }
    
    /**
     * Check if key exists
     */
    bool has(const std::string& key) const {
        return data_.contains(key);
    }
    
    /**
     * Get underlying JSON object
     */
    const json& data() const { return data_; }
    
private:
    json data_;
};

} // namespace cashew::utils
