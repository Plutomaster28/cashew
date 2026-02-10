#pragma once

#include "cashew/common.hpp"
#include <stdexcept>
#include <string>
#include <variant>
#include <optional>
#include <functional>

namespace cashew {

// Error codes for structured error handling
enum class ErrorCode {
    // Generic errors
    Success = 0,
    Unknown,
    InvalidArgument,
    OutOfRange,
    NotImplemented,
    
    // Cryptography errors
    CryptoInitFailed,
    CryptoSignatureFailed,
    CryptoVerificationFailed,
    CryptoEncryptionFailed,
    CryptoDecryptionFailed,
    CryptoKeyGenerationFailed,
    InvalidPublicKey,
    InvalidSecretKey,
    
    // Network errors
    NetworkConnectionFailed,
    NetworkTimeout,
    NetworkDisconnected,
    NetworkInvalidMessage,
    NetworkPeerNotFound,
    NetworkHandshakeFailed,
    
    // Storage errors
    StorageNotFound,
    StorageReadFailed,
    StorageWriteFailed,
    StorageCorrupted,
    StorageQuotaExceeded,
    
    // Protocol errors
    ProtocolInvalidMessage,
    ProtocolVersionMismatch,
    ProtocolAuthenticationFailed,
    ProtocolPermissionDenied,
    
    // PoW errors
    PoWInsufficientDifficulty,
    PoWInvalidSolution,
    PoWTimeoutExpired,
    
    // Ledger errors
    LedgerInvalidEvent,
    LedgerForkDetected,
    LedgerConflict,
    
    // Thing errors
    ThingNotFound,
    ThingSizeLimitExceeded,
    ThingInvalidHash,
    
    // Key errors
    KeyNotFound,
    KeyExpired,
    KeyInvalidPermission,
    KeyQuotaExceeded,
    
    // Reputation errors
    ReputationTooLow,
    ReputationInvalidAttestation,
    
    // Serialization errors
    SerializationFailed,
    DeserializationFailed,
    InvalidFormat
};

// Convert error code to string
const char* error_code_to_string(ErrorCode code);

// Error class with structured information
class Error {
public:
    Error(ErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {}
    
    Error(ErrorCode code, std::string message, std::string details)
        : code_(code), message_(std::move(message)), details_(std::move(details)) {}
    
    ErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }
    const std::string& details() const { return details_; }
    
    std::string to_string() const;
    
private:
    ErrorCode code_;
    std::string message_;
    std::string details_;
};

// Result type for error handling (similar to Rust's Result<T, E>)
template<typename T>
class Result {
public:
    // Success constructor
    static Result Ok(T value) {
        return Result(std::move(value));
    }
    
    // Error constructor
    static Result Err(Error error) {
        return Result(std::move(error));
    }
    
    // Error constructor with code and message
    static Result Err(ErrorCode code, const std::string& message) {
        return Result(Error(code, message));
    }
    
    // Check if result contains a value
    bool is_ok() const { return std::holds_alternative<T>(value_); }
    bool is_err() const { return std::holds_alternative<Error>(value_); }
    
    // Get the value (throws if error)
    T& value() {
        if (is_err()) {
            throw std::runtime_error("Called value() on error Result: " + error().to_string());
        }
        return std::get<T>(value_);
    }
    
    const T& value() const {
        if (is_err()) {
            throw std::runtime_error("Called value() on error Result: " + error().to_string());
        }
        return std::get<T>(value_);
    }
    
    // Get the error (throws if ok)
    const Error& error() const {
        if (is_ok()) {
            throw std::runtime_error("Called error() on ok Result");
        }
        return std::get<Error>(value_);
    }
    
    // Get value or default
    T value_or(T default_value) const {
        if (is_ok()) {
            return std::get<T>(value_);
        }
        return default_value;
    }
    
    // Get value or compute default
    template<typename F>
    T value_or_else(F&& func) const {
        if (is_ok()) {
            return std::get<T>(value_);
        }
        return func(std::get<Error>(value_));
    }
    
    // Unwrap (throws if error)
    T unwrap() {
        return value();
    }
    
    // Unwrap or throw custom error
    T expect(const std::string& message) {
        if (is_err()) {
            throw std::runtime_error(message + ": " + error().to_string());
        }
        return value();
    }
    
    // Map the value if ok
    template<typename F>
    auto map(F&& func) -> Result<decltype(func(std::declval<T>()))> {
        using U = decltype(func(std::declval<T>()));
        if (is_ok()) {
            return Result<U>::Ok(func(value()));
        }
        return Result<U>::Err(error());
    }
    
    // Map the error if err
    Result<T> map_err(std::function<Error(const Error&)> func) {
        if (is_err()) {
            return Result<T>::Err(func(error()));
        }
        return Result<T>::Ok(value());
    }
    
    // Convert to optional
    std::optional<T> ok() const {
        if (is_ok()) {
            return std::get<T>(value_);
        }
        return std::nullopt;
    }
    
    std::optional<Error> err() const {
        if (is_err()) {
            return std::get<Error>(value_);
        }
        return std::nullopt;
    }
    
private:
    explicit Result(T value) : value_(std::move(value)) {}
    explicit Result(Error error) : value_(std::move(error)) {}
    
    std::variant<T, Error> value_;
};

// Specialized Result<void> for operations that don't return a value
template<>
class Result<void> {
public:
    static Result Ok() {
        return Result(true);
    }
    
    static Result Err(Error error) {
        return Result(std::move(error));
    }
    
    static Result Err(ErrorCode code, const std::string& message) {
        return Result(Error(code, message));
    }
    
    bool is_ok() const { return !error_.has_value(); }
    bool is_err() const { return error_.has_value(); }
    
    const Error& error() const {
        if (!error_) {
            throw std::runtime_error("Called error() on ok Result");
        }
        return *error_;
    }
    
    void unwrap() {
        if (is_err()) {
            throw std::runtime_error("Called unwrap() on error Result: " + error().to_string());
        }
    }
    
    void expect(const std::string& message) {
        if (is_err()) {
            throw std::runtime_error(message + ": " + error().to_string());
        }
    }
    
private:
    explicit Result(bool) : error_(std::nullopt) {}
    explicit Result(Error error) : error_(std::move(error)) {}
    
    std::optional<Error> error_;
};

// Custom exception classes
class CashewException : public std::runtime_error {
public:
    CashewException(ErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}
    
    ErrorCode code() const { return code_; }
    
private:
    ErrorCode code_;
};

class CryptoException : public CashewException {
public:
    CryptoException(ErrorCode code, const std::string& message)
        : CashewException(code, "Crypto error: " + message) {}
};

class NetworkException : public CashewException {
public:
    NetworkException(ErrorCode code, const std::string& message)
        : CashewException(code, "Network error: " + message) {}
};

class StorageException : public CashewException {
public:
    StorageException(ErrorCode code, const std::string& message)
        : CashewException(code, "Storage error: " + message) {}
};

class ProtocolException : public CashewException {
public:
    ProtocolException(ErrorCode code, const std::string& message)
        : CashewException(code, "Protocol error: " + message) {}
};

// Utility macros for error handling
#define CASHEW_TRY(expr) \
    do { \
        auto __result = (expr); \
        if (__result.is_err()) { \
            return decltype(__result)::Err(__result.error()); \
        } \
    } while (0)

#define CASHEW_TRY_UNWRAP(var, expr) \
    auto __result_##var = (expr); \
    if (__result_##var.is_err()) { \
        return decltype(__result_##var)::Err(__result_##var.error()); \
    } \
    auto var = __result_##var.value();

} // namespace cashew
