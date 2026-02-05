#include "storage.hpp"
#include "utils/logger.hpp"
#include "crypto/blake3.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>

namespace cashew::storage {

// For now, use simple filesystem + in-memory map
// TODO: Integrate LevelDB/RocksDB for production
class Storage::Impl {
public:
    explicit Impl(const std::filesystem::path& data_dir)
        : data_dir_(data_dir)
        , content_dir_(data_dir / "content")
        , metadata_dir_(data_dir / "metadata")
    {
        // Create directories
        std::filesystem::create_directories(content_dir_);
        std::filesystem::create_directories(metadata_dir_);
        
        CASHEW_LOG_INFO("Storage initialized at: {}", data_dir_.string());
        CASHEW_LOG_INFO("Content directory: {}", content_dir_.string());
        CASHEW_LOG_INFO("Metadata directory: {}", metadata_dir_.string());
    }
    
    std::filesystem::path get_content_path(const ContentHash& hash) const {
        std::string hash_str = hash.to_string();
        // Use first 2 chars as subdirectory for better filesystem performance
        std::string subdir = hash_str.substr(0, 2);
        return content_dir_ / subdir / hash_str;
    }
    
    std::filesystem::path get_metadata_path(const std::string& key) const {
        // Simple key to filename (escape special chars)
        std::string safe_key = key;
        for (char& c : safe_key) {
            if (c == '/' || c == '\\' || c == ':') {
                c = '_';
            }
        }
        return metadata_dir_ / safe_key;
    }
    
    bool put_content(const ContentHash& hash, const bytes& data) {
        auto path = get_content_path(hash);
        
        // Create subdirectory
        std::filesystem::create_directories(path.parent_path());
        
        // Write file
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            CASHEW_LOG_ERROR("Failed to create content file: {}", path.string());
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        if (!file) {
            CASHEW_LOG_ERROR("Failed to write content file: {}", path.string());
            return false;
        }
        
        CASHEW_LOG_DEBUG("Stored content: {} ({} bytes)", hash.to_string(), data.size());
        return true;
    }
    
    std::optional<bytes> get_content(const ContentHash& hash) const {
        auto path = get_content_path(hash);
        
        if (!std::filesystem::exists(path)) {
            return std::nullopt;
        }
        
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            CASHEW_LOG_ERROR("Failed to open content file: {}", path.string());
            return std::nullopt;
        }
        
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        bytes data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        
        if (!file) {
            CASHEW_LOG_ERROR("Failed to read content file: {}", path.string());
            return std::nullopt;
        }
        
        return data;
    }
    
    bool has_content(const ContentHash& hash) const {
        return std::filesystem::exists(get_content_path(hash));
    }
    
    bool delete_content(const ContentHash& hash) {
        auto path = get_content_path(hash);
        
        if (!std::filesystem::exists(path)) {
            return false;
        }
        
        std::error_code ec;
        bool result = std::filesystem::remove(path, ec);
        
        if (!result || ec) {
            CASHEW_LOG_ERROR("Failed to delete content: {} ({})", 
                           path.string(), ec.message());
            return false;
        }
        
        return true;
    }
    
    bool put_metadata(const std::string& key, const bytes& value) {
        auto path = get_metadata_path(key);
        
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            CASHEW_LOG_ERROR("Failed to create metadata file: {}", path.string());
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(value.data()), value.size());
        return file.good();
    }
    
    std::optional<bytes> get_metadata(const std::string& key) const {
        auto path = get_metadata_path(key);
        
        if (!std::filesystem::exists(path)) {
            return std::nullopt;
        }
        
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::nullopt;
        }
        
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        bytes data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        
        return file ? std::optional<bytes>(data) : std::nullopt;
    }
    
    bool delete_metadata(const std::string& key) {
        auto path = get_metadata_path(key);
        
        if (!std::filesystem::exists(path)) {
            return false;
        }
        
        std::error_code ec;
        return std::filesystem::remove(path, ec);
    }
    
    std::vector<ContentHash> list_content() const {
        std::vector<ContentHash> hashes;
        
        if (!std::filesystem::exists(content_dir_)) {
            return hashes;
        }
        
        // Traverse subdirectories
        for (const auto& subdir : std::filesystem::directory_iterator(content_dir_)) {
            if (!subdir.is_directory()) continue;
            
            for (const auto& entry : std::filesystem::directory_iterator(subdir)) {
                if (entry.is_regular_file()) {
                    std::string hash_str = entry.path().filename().string();
                    auto hash = ContentHash::from_string(hash_str);
                    hashes.push_back(hash);
                }
            }
        }
        
        return hashes;
    }
    
    size_t total_size() const {
        size_t total = 0;
        
        if (!std::filesystem::exists(content_dir_)) {
            return 0;
        }
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(content_dir_)) {
            if (entry.is_regular_file()) {
                total += entry.file_size();
            }
        }
        
        return total;
    }
    
    size_t item_count() const {
        return list_content().size();
    }
    
private:
    std::filesystem::path data_dir_;
    std::filesystem::path content_dir_;
    std::filesystem::path metadata_dir_;
};

// Storage implementation
Storage::Storage(const std::filesystem::path& data_dir)
    : impl_(std::make_unique<Impl>(data_dir))
{}

Storage::~Storage() = default;
Storage::Storage(Storage&&) noexcept = default;
Storage& Storage::operator=(Storage&&) noexcept = default;

bool Storage::put_content(const ContentHash& content_hash, const bytes& data) {
    return impl_->put_content(content_hash, data);
}

std::optional<bytes> Storage::get_content(const ContentHash& content_hash) const {
    return impl_->get_content(content_hash);
}

bool Storage::has_content(const ContentHash& content_hash) const {
    return impl_->has_content(content_hash);
}

bool Storage::delete_content(const ContentHash& content_hash) {
    return impl_->delete_content(content_hash);
}

bool Storage::put_metadata(const std::string& key, const bytes& value) {
    return impl_->put_metadata(key, value);
}

std::optional<bytes> Storage::get_metadata(const std::string& key) const {
    return impl_->get_metadata(key);
}

bool Storage::delete_metadata(const std::string& key) {
    return impl_->delete_metadata(key);
}

std::vector<ContentHash> Storage::list_content() const {
    return impl_->list_content();
}

size_t Storage::total_size() const {
    return impl_->total_size();
}

size_t Storage::item_count() const {
    return impl_->item_count();
}

void Storage::compact() {
    CASHEW_LOG_INFO("Storage compaction not implemented for filesystem backend");
    // TODO: Implement when using LevelDB/RocksDB
}

} // namespace cashew::storage
