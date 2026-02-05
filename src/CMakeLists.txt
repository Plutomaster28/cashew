# Collect source files
set(CASHEW_SOURCES
    common.cpp
    
    # Crypto
    crypto/ed25519.cpp
    crypto/x25519.cpp
    crypto/blake3.cpp
    crypto/chacha20poly1305.cpp
    crypto/argon2.cpp
    crypto/random.cpp
    
    # Utils
    utils/logger.cpp
    utils/config.cpp
    
    # Storage
    storage/storage.cpp
    
    # Core
    core/node/node.cpp
    core/node/node_identity.cpp
    core/thing/thing.cpp
    core/keys/key.cpp
    core/pow/pow.cpp
    core/ledger/ledger.cpp
    core/ledger/state.cpp
    core/reputation/reputation.cpp
    core/postake/postake.cpp
    core/decay/decay.cpp
    security/access.cpp
    
    # Network
    network/network.cpp
    network/session.cpp
    network/gossip.cpp
    network/router.cpp
    network/peer.cpp
    network/ledger_sync.cpp
    security/onion_routing.cpp
    security/ip_protection.cpp
    security/attack_prevention.cpp
    gateway/gateway_server.cpp
    gateway/websocket_handler.cpp
    gateway/content_renderer.cpp
)

# Create core library
add_library(cashew_core STATIC ${CASHEW_SOURCES})

# Include directories
target_include_directories(cashew_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link dependencies
target_link_libraries(cashew_core
    PUBLIC
        PkgConfig::sodium
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        blake3
)

# Platform-specific
if(WIN32)
    # Windows socket libraries for cpp-httplib
    target_link_libraries(cashew_core PUBLIC ws2_32 wsock32)
    # OpenSSL for HTTPS support
    find_package(OpenSSL REQUIRED)
    target_link_libraries(cashew_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(cashew_core PUBLIC pthread)
    # OpenSSL for HTTPS support on Unix
    find_package(OpenSSL REQUIRED)
    target_link_libraries(cashew_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

# Executable - Cashew Node
add_executable(cashew_node main.cpp)

target_link_libraries(cashew_node
    PRIVATE
        cashew_core
)

# Installation
install(TARGETS cashew_core cashew_node
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
