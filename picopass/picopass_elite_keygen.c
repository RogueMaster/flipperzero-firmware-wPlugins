#include "picopass_elite_keygen.h"

/* Based on https://youtu.be/MKSXSKQHz6o?si=DEKkW60x858pUI0a&t=600 */

#define INITIAL_SEED 0x429080

uint32_t seed = INITIAL_SEED;
uint8_t key_state[8];
bool prepared = false;

void picopass_elite_reset() {
    memset(key_state, 0, sizeof(key_state));
    seed = INITIAL_SEED;
    prepared = false;
}

uint32_t picopass_elite_lcg() {
    uint32_t mod = 0x1000000; // 2^24,
    uint32_t a = 0xFD43FD;
    uint32_t c = 0xC39EC3;

    return (a * seed + c) % mod;
}

uint32_t picopass_elite_rng() {
    seed = picopass_elite_lcg();
    return seed;
}

uint8_t picopass_elite_nextByte() {
    return (picopass_elite_rng() >> 16) & 0xFF;
}

void picopass_elite_nextKey(uint8_t* key) {
    if(prepared) {
        for(size_t i = 0; i < 7; i++) {
            key_state[i] = key_state[i + 1];
        }
        key_state[7] = picopass_elite_nextByte();
    } else {
        for(size_t i = 0; i < 8; i++) {
            key_state[i] = picopass_elite_nextByte();
        }
        prepared = true;
    }
    memcpy(key, key_state, 8);
}

/*
int main() {
    size_t limit = 700;

    for (size_t i = 0; i < limit; i++) {
        nextKey();
        printKey(key);
        // printf("%04lx: %08x\n", i, nextByte());
    }
    return 0;
}
*/
