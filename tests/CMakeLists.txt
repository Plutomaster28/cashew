# Test CMake configuration

find_package(GTest CONFIG REQUIRED)

# Test for crypto
add_executable(test_crypto test_crypto.cpp)
target_link_libraries(test_crypto
    PRIVATE
        cashew_core
        GTest::gtest
        GTest::gtest_main
)

# Test for identity
add_executable(test_identity test_identity.cpp)
target_link_libraries(test_identity
    PRIVATE
        cashew_core
        GTest::gtest
        GTest::gtest_main
)

# Add tests
add_test(NAME CryptoTests COMMAND test_crypto)
add_test(NAME IdentityTests COMMAND test_identity)
