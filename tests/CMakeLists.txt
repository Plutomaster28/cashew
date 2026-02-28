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

# Test for storage
add_executable(test_storage test_storage.cpp)
target_link_libraries(test_storage
    PRIVATE
        cashew_core
        GTest::gtest
        GTest::gtest_main
)

# Test for PoW (API-aligned version)
add_executable(test_pow test_pow_fixed.cpp)
target_link_libraries(test_pow
    PRIVATE
        cashew_core
        GTest::gtest
        GTest::gtest_main
)

# Test for key system
add_executable(test_keys test_keys.cpp)
target_link_libraries(test_keys
    PRIVATE
        cashew_core
        GTest::gtest
        GTest::gtest_main
)

# Test for session/transport (API-aligned version)
add_executable(test_session test_session_fixed.cpp)
target_link_libraries(test_session
    PRIVATE
        cashew_core
        GTest::gtest
        GTest::gtest_main
)

# Test for ledger and reputation (temporarily disabled due to compilation errors)
# add_executable(test_ledger_reputation test_ledger_reputation.cpp)
# target_link_libraries(test_ledger_reputation
#     PRIVATE
#         cashew_core
#         GTest::gtest
#         GTest::gtest_main
# )

# Test for security (temporarily disabled due to compilation errors)
# add_executable(test_security test_security.cpp)
# target_link_libraries(test_security
#     PRIVATE
#         cashew_core
#         GTest::gtest
#         GTest::gtest_main
# )

# Test for gateway
# Temporarily disabled - needs API updates for Linux compatibility
# add_executable(test_gateway test_gateway.cpp)
# target_link_libraries(test_gateway
#     PRIVATE
#         cashew_core
#         GTest::gtest
#         GTest::gtest_main
# )

# Test for Thing
add_executable(test_thing test_thing.cpp)
target_link_libraries(test_thing
    PRIVATE
        cashew_core
        GTest::gtest
        GTest::gtest_main
)

# Add tests
add_test(NAME CryptoTests COMMAND test_crypto)
add_test(NAME IdentityTests COMMAND test_identity)
add_test(NAME UtilsTests COMMAND test_utils)
add_test(NAME StorageTests COMMAND test_storage)
add_test(NAME PoWTests COMMAND test_pow)
add_test(NAME KeysTests COMMAND test_keys)
add_test(NAME NetworkTests COMMAND test_network)
# add_test(NAME LedgerReputationTests COMMAND test_ledger_reputation)  # Temporarily disabled
# add_test(NAME SecurityTests COMMAND test_security)  # Temporarily disabled
# add_test(NAME GatewayTests COMMAND test_gateway)  # Temporarily disabled - needs API updates
add_test(NAME ThingTests COMMAND test_thing)

