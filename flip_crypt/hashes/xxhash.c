#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "xxhash.h"

uint64_t XXH_rotl64(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}

uint64_t XXH64(const void* input, size_t len, uint64_t seed) {
    const uint64_t PRIME1 = 11400714785074694791ULL;
    const uint64_t PRIME2 = 14029467366897019727ULL;
    const uint64_t PRIME3 = 1609587929392839161ULL;
    const uint64_t PRIME4 = 9650029242287828579ULL;
    const uint64_t PRIME5 = 2870177450012600261ULL;

    const uint8_t* p = (const uint8_t*)input;
    const uint8_t* end = p + len;
    uint64_t h;

    if (len >= 32) {
        uint64_t v1 = seed + PRIME1 + PRIME2;
        uint64_t v2 = seed + PRIME2;
        uint64_t v3 = seed + 0;
        uint64_t v4 = seed - PRIME1;

        const uint8_t* const limit = end - 32;
        do {
            v1 = XXH_rotl64(v1 + (*(uint64_t*)p) * PRIME2, 31) * PRIME1; p += 8;
            v2 = XXH_rotl64(v2 + (*(uint64_t*)p) * PRIME2, 31) * PRIME1; p += 8;
            v3 = XXH_rotl64(v3 + (*(uint64_t*)p) * PRIME2, 31) * PRIME1; p += 8;
            v4 = XXH_rotl64(v4 + (*(uint64_t*)p) * PRIME2, 31) * PRIME1; p += 8;
        } while (p <= limit);

        h = XXH_rotl64(v1, 1) + XXH_rotl64(v2, 7) + XXH_rotl64(v3, 12) + XXH_rotl64(v4, 18);

        v1 *= PRIME2; v1 = XXH_rotl64(v1, 31); v1 *= PRIME1; h ^= v1; h = h * PRIME1 + PRIME4;
        v2 *= PRIME2; v2 = XXH_rotl64(v2, 31); v2 *= PRIME1; h ^= v2; h = h * PRIME1 + PRIME4;
        v3 *= PRIME2; v3 = XXH_rotl64(v3, 31); v3 *= PRIME1; h ^= v3; h = h * PRIME1 + PRIME4;
        v4 *= PRIME2; v4 = XXH_rotl64(v4, 31); v4 *= PRIME1; h ^= v4; h = h * PRIME1 + PRIME4;
    } else {
        h = seed + PRIME5;
    }

    h += (uint64_t)len;

    while (p + 8 <= end) {
        uint64_t k1 = (*(uint64_t*)p) * PRIME2;
        k1 = XXH_rotl64(k1, 31);
        k1 *= PRIME1;
        h ^= k1;
        h = XXH_rotl64(h, 27) * PRIME1 + PRIME4;
        p += 8;
    }

    if (p + 4 <= end) {
        h ^= (uint64_t)(*(uint32_t*)p) * PRIME1;
        h = XXH_rotl64(h, 23) * PRIME2 + PRIME3;
        p += 4;
    }

    while (p < end) {
        h ^= (*p) * PRIME5;
        h = XXH_rotl64(h, 11) * PRIME1;
        ++p;
    }

    h ^= h >> 33;
    h *= PRIME2;
    h ^= h >> 29;
    h *= PRIME3;
    h ^= h >> 32;

    return h;
}