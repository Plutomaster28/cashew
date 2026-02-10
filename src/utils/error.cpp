#include "cashew/error.hpp"
#include <sstream>

namespace cashew {

const char* error_code_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::Unknown: return "Unknown error";
        case ErrorCode::InvalidArgument: return "Invalid argument";
        case ErrorCode::OutOfRange: return "Out of range";
        case ErrorCode::NotImplemented: return "Not implemented";
        
        case ErrorCode::CryptoInitFailed: return "Crypto initialization failed";
        case ErrorCode::CryptoSignatureFailed: return "Signature failed";
        case ErrorCode::CryptoVerificationFailed: return "Verification failed";
        case ErrorCode::CryptoEncryptionFailed: return "Encryption failed";
        case ErrorCode::CryptoDecryptionFailed: return "Decryption failed";
        case ErrorCode::CryptoKeyGenerationFailed: return "Key generation failed";
        case ErrorCode::InvalidPublicKey: return "Invalid public key";
        case ErrorCode::InvalidSecretKey: return "Invalid secret key";
        
        case ErrorCode::NetworkConnectionFailed: return "Connection failed";
        case ErrorCode::NetworkTimeout: return "Network timeout";
        case ErrorCode::NetworkDisconnected: return "Disconnected";
        case ErrorCode::NetworkInvalidMessage: return "Invalid network message";
        case ErrorCode::NetworkPeerNotFound: return "Peer not found";
        case ErrorCode::NetworkHandshakeFailed: return "Handshake failed";
        
        case ErrorCode::StorageNotFound: return "Not found in storage";
        case ErrorCode::StorageReadFailed: return "Storage read failed";
        case ErrorCode::StorageWriteFailed: return "Storage write failed";
        case ErrorCode::StorageCorrupted: return "Storage corrupted";
        case ErrorCode::StorageQuotaExceeded: return "Storage quota exceeded";
        
        case ErrorCode::ProtocolInvalidMessage: return "Invalid protocol message";
        case ErrorCode::ProtocolVersionMismatch: return "Protocol version mismatch";
        case ErrorCode::ProtocolAuthenticationFailed: return "Authentication failed";
        case ErrorCode::ProtocolPermissionDenied: return "Permission denied";
        
        case ErrorCode::PoWInsufficientDifficulty: return "Insufficient PoW difficulty";
        case ErrorCode::PoWInvalidSolution: return "Invalid PoW solution";
        case ErrorCode::PoWTimeoutExpired: return "PoW timeout expired";
        
        case ErrorCode::LedgerInvalidEvent: return "Invalid ledger event";
        case ErrorCode::LedgerForkDetected: return "Ledger fork detected";
        case ErrorCode::LedgerConflict: return "Ledger conflict";
        
        case ErrorCode::ThingNotFound: return "Thing not found";
        case ErrorCode::ThingSizeLimitExceeded: return "Thing size limit exceeded";
        case ErrorCode::ThingInvalidHash: return "Invalid Thing hash";
        
        case ErrorCode::KeyNotFound: return "Key not found";
        case ErrorCode::KeyExpired: return "Key expired";
        case ErrorCode::KeyInvalidPermission: return "Invalid key permission";
        case ErrorCode::KeyQuotaExceeded: return "Key quota exceeded";
        
        case ErrorCode::ReputationTooLow: return "Reputation too low";
        case ErrorCode::ReputationInvalidAttestation: return "Invalid attestation";
        
        case ErrorCode::SerializationFailed: return "Serialization failed";
        case ErrorCode::DeserializationFailed: return "Deserialization failed";
        case ErrorCode::InvalidFormat: return "Invalid format";
        
        default: return "Unknown error code";
    }
}

std::string Error::to_string() const {
    std::ostringstream oss;
    oss << "[" << error_code_to_string(code_) << "] " << message_;
    if (!details_.empty()) {
        oss << " (" << details_ << ")";
    }
    return oss.str();
}

} // namespace cashew
