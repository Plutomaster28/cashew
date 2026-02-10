#pragma once

#include "cashew/common.hpp"
#include <map>
#include <variant>
#include <stdexcept>

namespace cashew {

// Serializable types
enum class SerializableType {
    Null,
    Bool,
    Int,
    UInt,
    Float,
    String,
    Binary,
    Array,
    Map
};

// Forward declaration
class Serializable;

// Serializable value type (for nested structures)
using SerializableValue = std::variant<
    std::monostate,
    bool,
    int64_t,
    uint64_t,
    double,
    std::string,
    bytes,
    std::vector<Serializable>,
    std::map<std::string, Serializable>
>;

// Serializable data container
class Serializable {
public:
    Serializable() : value_(std::monostate{}) {}
    
    // Constructors for different types
    Serializable(bool v) : value_(v) {}
    Serializable(int v) : value_(static_cast<int64_t>(v)) {}
    Serializable(int64_t v) : value_(v) {}
    Serializable(uint64_t v) : value_(v) {}
    Serializable(double v) : value_(v) {}
    Serializable(const std::string& v) : value_(v) {}
    Serializable(const char* v) : value_(std::string(v)) {}
    Serializable(const bytes& v) : value_(v) {}
    Serializable(const std::vector<Serializable>& v) : value_(v) {}
    Serializable(const std::map<std::string, Serializable>& v) : value_(v) {}
    
    // Type checking
    SerializableType type() const;
    bool is_null() const { return std::holds_alternative<std::monostate>(value_); }
    bool is_bool() const { return std::holds_alternative<bool>(value_); }
    bool is_int() const { return std::holds_alternative<int64_t>(value_); }
    bool is_uint() const { return std::holds_alternative<uint64_t>(value_); }
    bool is_float() const { return std::holds_alternative<double>(value_); }
    bool is_string() const { return std::holds_alternative<std::string>(value_); }
    bool is_binary() const { return std::holds_alternative<bytes>(value_); }
    bool is_array() const { return std::holds_alternative<std::vector<Serializable>>(value_); }
    bool is_map() const { return std::holds_alternative<std::map<std::string, Serializable>>(value_); }
    
    // Value getters (with type checking)
    bool as_bool() const;
    int64_t as_int() const;
    uint64_t as_uint() const;
    double as_float() const;
    const std::string& as_string() const;
    const bytes& as_binary() const;
    const std::vector<Serializable>& as_array() const;
    const std::map<std::string, Serializable>& as_map() const;
    
    // Array access
    Serializable& operator[](size_t index);
    const Serializable& operator[](size_t index) const;
    
    // Map access
    Serializable& operator[](const std::string& key);
    const Serializable& operator[](const std::string& key) const;
    
    // Array operations
    void push_back(const Serializable& value);
    size_t size() const;
    
private:
    SerializableValue value_;
};

// Serialization interface
class ISerializable {
public:
    virtual ~ISerializable() = default;
    
    // Serialize to Serializable format
    virtual Serializable serialize() const = 0;
    
    // Deserialize from Serializable format
    virtual void deserialize(const Serializable& data) = 0;
};

// Binary serialization format (simple MessagePack-like encoding)
// This can be replaced with actual MessagePack library integration later
class BinarySerializer {
public:
    // Serialize to binary format
    static bytes serialize(const Serializable& data);
    
    // Deserialize from binary format
    static Serializable deserialize(const bytes& data);
    
private:
    // Encoding helpers
    static void encode_type(bytes& output, SerializableType type);
    static void encode_int(bytes& output, int64_t value);
    static void encode_uint(bytes& output, uint64_t value);
    static void encode_float(bytes& output, double value);
    static void encode_string(bytes& output, const std::string& value);
    static void encode_binary(bytes& output, const bytes& value);
    static void encode_array(bytes& output, const std::vector<Serializable>& value);
    static void encode_map(bytes& output, const std::map<std::string, Serializable>& value);
    
    // Decoding helpers
    static SerializableType decode_type(const bytes& input, size_t& offset);
    static int64_t decode_int(const bytes& input, size_t& offset);
    static uint64_t decode_uint(const bytes& input, size_t& offset);
    static double decode_float(const bytes& input, size_t& offset);
    static std::string decode_string(const bytes& input, size_t& offset);
    static bytes decode_binary(const bytes& input, size_t& offset);
    static std::vector<Serializable> decode_array(const bytes& input, size_t& offset);
    static std::map<std::string, Serializable> decode_map(const bytes& input, size_t& offset);
    static Serializable decode_value(const bytes& input, size_t& offset);
};

// Helper macros for implementing serialization
#define CASHEW_SERIALIZABLE_FIELD(field_name, field) \
    data[field_name] = Serializable(field);

#define CASHEW_DESERIALIZE_FIELD(field_name, field, type) \
    if (data.is_map() && data[field_name].is_##type()) { \
        field = data[field_name].as_##type(); \
    }

} // namespace cashew
