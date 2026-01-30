# Cashew - Core C++ library
add_library(cashew_core STATIC)

# Core subdirectories
add_subdirectory(core)
add_subdirectory(crypto)
add_subdirectory(network)
add_subdirectory(storage)
add_subdirectory(gateway)
add_subdirectory(security)
add_subdirectory(utils)

# Link dependencies
target_link_libraries(cashew_core
    PUBLIC
        Threads::Threads
        ${CASHEW_CRYPTO_LIB}
    PRIVATE
        # spdlog::spdlog
        # nlohmann_json::nlohmann_json
)

# Include directories
target_include_directories(cashew_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

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
