#ifndef FIXED_MATH_H_
#define FIXED_MATH_H_

#include <stdint.h>

#define FIXED_SHIFT             7
#define FIXED_ONE               (1 << FIXED_SHIFT)
#define FIXED_HALF              (1 << (FIXED_SHIFT - 1))
#define FIXED_TO_INT(x)         ((x) >> FIXED_SHIFT)
#define FIXED_TO_INT_ROUNDED(x) (((x) + FIXED_HALF) >> FIXED_SHIFT)
#define INT_TO_FIXED(x)         ((x) << FIXED_SHIFT)
#define DEGREES_90              64
#define DEGREES_180             128
#define DEGREES_270             192
#define DEGREES_360             256

typedef int16_t fixed_t;
typedef uint8_t angle_t;

class FixedMath {
public:
    static fixed_t Sin(angle_t x);
    static inline fixed_t Cos(angle_t x) {
        return Sin((angle_t)(DEGREES_90 + (int16_t)x));
    }
};

int8_t clamp(int8_t x, int8_t lower, int8_t upper);
uint8_t getRandomNumber();
uint16_t getRandomNumber16();

#endif
