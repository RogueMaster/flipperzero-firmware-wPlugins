#include "random.h"

void RandomInitialize(RandomGenerator* generator, uint32_t seed) {
    generator->state = seed == 0 ? 0x6D2B79F5U : seed;
}

uint32_t RandomNext(RandomGenerator* generator) {
    uint32_t value = generator->state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;

    generator->state = value;
    return value;
}

int RandomRange(RandomGenerator* generator, int minimum, int maximum) {
    uint32_t range = (uint32_t)(maximum - minimum + 1);
    return minimum + (int)(RandomNext(generator) % range);
}
