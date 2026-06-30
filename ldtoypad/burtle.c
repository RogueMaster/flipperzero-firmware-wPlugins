#include "burtle.h"

// Helper function for rotation
static inline uint32_t rot(uint32_t a, int b) {
    return (a << b) | (a >> (32 - b));
}

// Constructor equivalent
void burtle_init(Burtle* b, uint32_t seed) {
    b->a = 0xf1ea5eed;
    b->b = b->c = b->d = seed;
    for(int i = 0; i < 42; ++i) {
        burtle_rand(b); // Initialize with 42 iterations
    }
}

// rand method
uint32_t burtle_rand(Burtle* b) {
    uint32_t e = b->a - rot(b->b, 21);
    b->a = b->b ^ rot(b->c, 19);
    b->b = b->c + rot(b->d, 6);
    b->c = b->d + e;
    b->d = e + b->a;
    return b->d;
}
