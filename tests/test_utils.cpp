#include "cashew/common.hpp"
#include "cashew/serialization.hpp"
#include "cashew/time_utils.hpp"
#include "cashew/error.hpp"
#include <iostream>
#include <cassert>

using namespace cashew;

void test_base64() {
    std::cout << "Testing Base64 encoding/decoding..." << std::endl;
    
    // Test 1: Simple string
    std::string test_str = "Hello, Cashew!";
    bytes test_bytes(test_str.begin(), test_str.end());
    std::string encoded = base64_encode(test_bytes);
    bytes decoded = base64_decode(encoded);
    
    assert(decoded == test_bytes);
    std::cout << "  ✓ Base64 encode/decode works" << std::endl;
}

void test_serialization() {
    std::cout << "Testing Serialization..." << std::endl;
    
    // Test 1: Simple types
    Serializable data;
    data["name"] = Serializable("Cashew Network");
    data["version"] = Serializable(1);
    data["active"] = Serializable(true);
    
    assert(data["name"].is_string());
    assert(data["version"].is_int());
    assert(data["active"].is_bool());
    assert(data["name"].as_string() == "Cashew Network");
    assert(data["version"].as_int() == 1);
    assert(data["active"].as_bool() == true);
    
    std::cout << "  ✓ Basic serialization types work" << std::endl;
    
    // Test 2: Binary serialization
    bytes binary = BinarySerializer::serialize(data);
    Serializable deserialized = BinarySerializer::deserialize(binary);
    
    assert(deserialized["name"].as_string() == "Cashew Network");
    assert(deserialized["version"].as_int() == 1);
    assert(deserialized["active"].as_bool() == true);
    
    std::cout << "  ✓ Binary serialization round-trip works" << std::endl;
}

void test_time_utils() {
    std::cout << "Testing Time utilities..." << std::endl;
    
    // Test 1: Timestamps
    uint64_t ts1 = time::timestamp_seconds();
    time::sleep_milliseconds(10);
    uint64_t ts2 = time::timestamp_seconds();
    assert(ts2 >= ts1);
    
    std::cout << "  ✓ Timestamps work" << std::endl;
    
    // Test 2: Timer
    time::Timer timer;
    time::sleep_milliseconds(50);
    double elapsed = timer.elapsed_seconds();
    assert(elapsed >= 0.04); // Should be at least 40ms
    
    std::cout << "  ✓ Timer works" << std::endl;
    
    // Test 3: Epoch manager
    time::EpochManager epoch_mgr;
    uint64_t current_epoch = epoch_mgr.current_epoch();
    assert(current_epoch > 0);
    
    std::cout << "  ✓ Epoch manager works" << std::endl;
    
    // Test 4: Rate limiter
    time::RateLimiter limiter(3, 1); // 3 operations per second
    assert(limiter.allow());
    assert(limiter.allow());
    assert(limiter.allow());
    assert(!limiter.allow()); // 4th should fail
    
    std::cout << "  ✓ Rate limiter works" << std::endl;
}

void test_error_handling() {
    std::cout << "Testing Error handling..." << std::endl;
    
    // Test 1: Result with success
    Result<int> ok_result = Result<int>::Ok(42);
    assert(ok_result.is_ok());
    assert(ok_result.value() == 42);
    
    std::cout << "  ✓ Result::Ok works" << std::endl;
    
    // Test 2: Result with error
    Result<int> err_result = Result<int>::Err(ErrorCode::InvalidArgument, "Test error");
    assert(err_result.is_err());
    assert(err_result.error().code() == ErrorCode::InvalidArgument);
    
    std::cout << "  ✓ Result::Err works" << std::endl;
    
    // Test 3: Result<void>
    Result<void> void_ok = Result<void>::Ok();
    assert(void_ok.is_ok());
    
    Result<void> void_err = Result<void>::Err(ErrorCode::Unknown, "Test");
    assert(void_err.is_err());
    
    std::cout << "  ✓ Result<void> works" << std::endl;
    
    // Test 4: Error code strings
    const char* err_str = error_code_to_string(ErrorCode::NetworkTimeout);
    assert(std::string(err_str) == "Network timeout");
    
    std::cout << "  ✓ Error code to string works" << std::endl;
}

int main() {
    std::cout << "=== Cashew Utilities Test ===" << std::endl << std::endl;
    
    try {
        test_base64();
        test_serialization();
        test_time_utils();
        test_error_handling();
        
        std::cout << std::endl << "✓ All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
