#pragma once

#include "cashew/common.hpp"

namespace cashew::crypto {

/**
 * Random number generation (CSPRNG)
 * Cryptographically secure random bytes
 */
class Random {
public:
    /**
     * Generate random bytes
     * @param size Number of bytes to generate
     * @return Random bytes
     */
    static bytes generate(size_t size);
    
    /**
     * Generate random 32-bit integer
     */
    static uint32_t generate_uint32();
    
    /**
     * Generate random 64-bit integer
     */
    static uint64_t generate_uint64();
    
    /**
     * Generate random bytes into existing buffer
     * @param buffer Buffer to fill
     * @param size Buffer size
     */
    static void generate_into(byte* buffer, size_t size);
    
    /**
     * Generate uniform random integer in range [0, upper_bound)
     * @param upper_bound Exclusive upper bound
     * @return Random integer
     */
    static uint32_t uniform(uint32_t upper_bound);
};

} // namespace cashew::crypto
