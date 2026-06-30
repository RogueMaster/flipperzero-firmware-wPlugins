#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t a, b, c, d;
} Burtle;

void burtle_init(Burtle* b, uint32_t seed);
uint32_t burtle_rand(Burtle* b);

#ifdef __cplusplus
}
#endif