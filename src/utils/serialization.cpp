#include "cashew/serialization.hpp"
#include <cstring>
#include <stdexcept>

namespace cashew {

// Serializable implementation
SerializableType Serializable::type() const {
    if (std::holds_alternative<std::monostate>(value_)) return SerializableType::Null;
    if (std::holds_alternative<bool>(value_)) return SerializableType::Bool;
    if (std::holds_alternative<int64_t>(value_)) return SerializableType::Int;
    if (std::holds_alternative<uint64_t>(value_)) return SerializableType::UInt;
    if (std::holds_alternative<double>(value_)) return SerializableType::Float;
    if (std::holds_alternative<std::string>(value_)) return SerializableType::String;
    if (std::holds_alternative<bytes>(value_)) return SerializableType::Binary;
    if (std::holds_alternative<std::vector<Serializable>>(value_)) return SerializableType::Array;
    if (std::holds_alternative<std::map<std::string, Serializable>>(value_)) return SerializableType::Map;
    return SerializableType::Null;
}

bool Serializable::as_bool() const {
    if (!is_bool()) throw std::runtime_error("Not a boolean");
    return std::get<bool>(value_);
}

int64_t Serializable::as_int() const {
    if (!is_int()) throw std::runtime_error("Not an integer");
    return std::get<int64_t>(value_);
}

uint64_t Serializable::as_uint() const {
    if (!is_uint()) throw std::runtime_error("Not an unsigned integer");
    return std::get<uint64_t>(value_);
}

double Serializable::as_float() const {
    if (!is_float()) throw std::runtime_error("Not a float");
    return std::get<double>(value_);
}

const std::string& Serializable::as_string() const {
    if (!is_string()) throw std::runtime_error("Not a string");
    return std::get<std::string>(value_);
}

const bytes& Serializable::as_binary() const {
    if (!is_binary()) throw std::runtime_error("Not binary data");
    return std::get<bytes>(value_);
}

const std::vector<Serializable>& Serializable::as_array() const {
    if (!is_array()) throw std::runtime_error("Not an array");
    return std::get<std::vector<Serializable>>(value_);
}

const std::map<std::string, Serializable>& Serializable::as_map() const {
    if (!is_map()) throw std::runtime_error("Not a map");
    return std::get<std::map<std::string, Serializable>>(value_);
}

Serializable& Serializable::operator[](size_t index) {
    if (!is_array()) {
        value_ = std::vector<Serializable>();
    }
    auto& arr = std::get<std::vector<Serializable>>(value_);
    if (index >= arr.size()) {
        arr.resize(index + 1);
    }
    return arr[index];
}

const Serializable& Serializable::operator[](size_t index) const {
    if (!is_array()) throw std::runtime_error("Not an array");
    const auto& arr = std::get<std::vector<Serializable>>(value_);
    if (index >= arr.size()) throw std::out_of_range("Array index out of range");
    return arr[index];
}

Serializable& Serializable::operator[](const std::string& key) {
    if (!is_map()) {
        value_ = std::map<std::string, Serializable>();
    }
    return std::get<std::map<std::string, Serializable>>(value_)[key];
}

const Serializable& Serializable::operator[](const std::string& key) const {
    if (!is_map()) throw std::runtime_error("Not a map");
    const auto& m = std::get<std::map<std::string, Serializable>>(value_);
    auto it = m.find(key);
    if (it == m.end()) throw std::out_of_range("Key not found in map");
    return it->second;
}

void Serializable::push_back(const Serializable& value) {
    if (!is_array()) {
        value_ = std::vector<Serializable>();
    }
    std::get<std::vector<Serializable>>(value_).push_back(value);
}

size_t Serializable::size() const {
    if (is_array()) {
        return std::get<std::vector<Serializable>>(value_).size();
    } else if (is_map()) {
        return std::get<std::map<std::string, Serializable>>(value_).size();
    }
    return 0;
}

// BinarySerializer implementation
bytes BinarySerializer::serialize(const Serializable& data) {
    bytes output;
    auto type = data.type();
    encode_type(output, type);
    
    switch (type) {
        case SerializableType::Null:
            break;
        case SerializableType::Bool:
            output.push_back(data.as_bool() ? 1 : 0);
            break;
        case SerializableType::Int:
            encode_int(output, data.as_int());
            break;
        case SerializableType::UInt:
            encode_uint(output, data.as_uint());
            break;
        case SerializableType::Float:
            encode_float(output, data.as_float());
            break;
        case SerializableType::String:
            encode_string(output, data.as_string());
            break;
        case SerializableType::Binary:
            encode_binary(output, data.as_binary());
            break;
        case SerializableType::Array:
            encode_array(output, data.as_array());
            break;
        case SerializableType::Map:
            encode_map(output, data.as_map());
            break;
    }
    
    return output;
}

Serializable BinarySerializer::deserialize(const bytes& data) {
    size_t offset = 0;
    return decode_value(data, offset);
}

void BinarySerializer::encode_type(bytes& output, SerializableType type) {
    output.push_back(static_cast<byte>(type));
}

void BinarySerializer::encode_int(bytes& output, int64_t value) {
    for (int i = 0; i < 8; ++i) {
        output.push_back(static_cast<byte>((value >> (i * 8)) & 0xFF));
    }
}

void BinarySerializer::encode_uint(bytes& output, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        output.push_back(static_cast<byte>((value >> (i * 8)) & 0xFF));
    }
}

void BinarySerializer::encode_float(bytes& output, double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(double));
    encode_uint(output, bits);
}

void BinarySerializer::encode_string(bytes& output, const std::string& value) {
    encode_uint(output, value.size());
    output.insert(output.end(), value.begin(), value.end());
}

void BinarySerializer::encode_binary(bytes& output, const bytes& value) {
    encode_uint(output, value.size());
    output.insert(output.end(), value.begin(), value.end());
}

void BinarySerializer::encode_array(bytes& output, const std::vector<Serializable>& value) {
    encode_uint(output, value.size());
    for (const auto& item : value) {
        auto encoded = serialize(item);
        output.insert(output.end(), encoded.begin(), encoded.end());
    }
}

void BinarySerializer::encode_map(bytes& output, const std::map<std::string, Serializable>& value) {
    encode_uint(output, value.size());
    for (const auto& [key, val] : value) {
        encode_string(output, key);
        auto encoded = serialize(val);
        output.insert(output.end(), encoded.begin(), encoded.end());
    }
}

SerializableType BinarySerializer::decode_type(const bytes& input, size_t& offset) {
    if (offset >= input.size()) throw std::runtime_error("Unexpected end of data");
    return static_cast<SerializableType>(input[offset++]);
}

int64_t BinarySerializer::decode_int(const bytes& input, size_t& offset) {
    if (offset + 8 > input.size()) throw std::runtime_error("Unexpected end of data");
    int64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<int64_t>(input[offset++]) << (i * 8));
    }
    return value;
}

uint64_t BinarySerializer::decode_uint(const bytes& input, size_t& offset) {
    if (offset + 8 > input.size()) throw std::runtime_error("Unexpected end of data");
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<uint64_t>(input[offset++]) << (i * 8));
    }
    return value;
}

double BinarySerializer::decode_float(const bytes& input, size_t& offset) {
    uint64_t bits = decode_uint(input, offset);
    double value;
    std::memcpy(&value, &bits, sizeof(double));
    return value;
}

std::string BinarySerializer::decode_string(const bytes& input, size_t& offset) {
    uint64_t len = decode_uint(input, offset);
    if (offset + len > input.size()) throw std::runtime_error("Unexpected end of data");
    std::string value(reinterpret_cast<const char*>(&input[offset]), len);
    offset += len;
    return value;
}

bytes BinarySerializer::decode_binary(const bytes& input, size_t& offset) {
    uint64_t len = decode_uint(input, offset);
    if (offset + len > input.size()) throw std::runtime_error("Unexpected end of data");
    bytes value(input.begin() + offset, input.begin() + offset + len);
    offset += len;
    return value;
}

std::vector<Serializable> BinarySerializer::decode_array(const bytes& input, size_t& offset) {
    uint64_t len = decode_uint(input, offset);
    std::vector<Serializable> value;
    value.reserve(len);
    for (uint64_t i = 0; i < len; ++i) {
        value.push_back(decode_value(input, offset));
    }
    return value;
}

std::map<std::string, Serializable> BinarySerializer::decode_map(const bytes& input, size_t& offset) {
    uint64_t len = decode_uint(input, offset);
    std::map<std::string, Serializable> value;
    for (uint64_t i = 0; i < len; ++i) {
        std::string key = decode_string(input, offset);
        value[key] = decode_value(input, offset);
    }
    return value;
}

Serializable BinarySerializer::decode_value(const bytes& input, size_t& offset) {
    auto type = decode_type(input, offset);
    
    switch (type) {
        case SerializableType::Null:
            return Serializable();
        case SerializableType::Bool:
            if (offset >= input.size()) throw std::runtime_error("Unexpected end of data");
            return Serializable(input[offset++] != 0);
        case SerializableType::Int:
            return Serializable(decode_int(input, offset));
        case SerializableType::UInt:
            return Serializable(decode_uint(input, offset));
        case SerializableType::Float:
            return Serializable(decode_float(input, offset));
        case SerializableType::String:
            return Serializable(decode_string(input, offset));
        case SerializableType::Binary:
            return Serializable(decode_binary(input, offset));
        case SerializableType::Array:
            return Serializable(decode_array(input, offset));
        case SerializableType::Map:
            return Serializable(decode_map(input, offset));
        default:
            throw std::runtime_error("Unknown serializable type");
    }
}

} // namespace cashew
