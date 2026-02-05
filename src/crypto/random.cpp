#include "random.hpp"
#include <sodium.h>

namespace cashew::crypto {

bytes Random::generate(size_t size) {
    bytes result(size);
    randombytes_buf(result.data(), size);
    return result;
}

uint32_t Random::generate_uint32() {
    return randombytes_random();
}

uint64_t Random::generate_uint64() {
    uint64_t high = randombytes_random();
    uint64_t low = randombytes_random();
    return (high << 32) | low;
}

void Random::generate_into(byte* buffer, size_t size) {
    randombytes_buf(buffer, size);
}

uint32_t Random::uniform(uint32_t upper_bound) {
    return randombytes_uniform(upper_bound);
}

} // namespace cashew::crypto
