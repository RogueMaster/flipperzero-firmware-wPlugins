#include "game/Engine.h"
#include "game/Defines.h"
#include "game/FixedMath.h"
#include "game/TrigLUT.h"

fixed_t FixedMath::Sin(angle_t x) {
    if(x <= DEGREES_90) {
        return (fixed_t)(pgm_read_byte(&TrigLUT[x]));
    } else if(x <= DEGREES_180) {
        return (fixed_t)(pgm_read_byte(&TrigLUT[DEGREES_180 - x]));
    } else if(x <= DEGREES_270) {
        return -1 * (fixed_t)(pgm_read_byte(&TrigLUT[x - DEGREES_180]));
    } else {
        return -1 * (fixed_t)(pgm_read_byte(&TrigLUT[DEGREES_360 - x]));
    }
}

int8_t clamp(int8_t x, int8_t lower, int8_t upper) {
    if(x < lower) return lower;
    if(x > upper) return upper;
    return x;
}

uint8_t getRandomNumber() {
    return getRandomNumber16() & 0xff;
}

uint16_t getRandomNumber16() {
    static uint16_t randVal = 0xABC;

    uint16_t lsb = randVal & 1;
    randVal >>= 1;
    if(lsb == 1) {
        randVal ^= 0xB400u;
    }

    return (randVal - 1);
}
