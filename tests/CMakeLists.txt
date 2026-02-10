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

# Test for utilities
add_executable(test_utils test_utils.cpp)
target_link_libraries(test_utils
    PRIVATE
        cashew_core
)

# Add tests
add_test(NAME CryptoTests COMMAND test_crypto)
add_test(NAME IdentityTests COMMAND test_identity)
add_test(NAME UtilsTests COMMAND test_utils)
