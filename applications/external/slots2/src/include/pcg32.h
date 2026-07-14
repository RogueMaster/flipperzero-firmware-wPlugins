/*
 * File: pcg32.h
 * Author: vh8t
 * Created: 28/12/2025
 */

#ifndef PCG32_H
#define PCG32_H

#include <stdint.h>

typedef struct {
    uint64_t state;
    uint64_t inc;
} pcg32_t;

void pcg32_seed(pcg32_t* rng);
uint32_t pcg32_random(pcg32_t* rng);
uint32_t pcg32_random_bound(pcg32_t* rng, uint32_t bound);

#endif // PCG32_H
