#ifndef CARTO_FIXEDPT_H
#define CARTO_FIXEDPT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t carto_fix;

#define CARTO_FIX_SHIFT 16
#define CARTO_FIX_ONE   ((carto_fix)(1 << CARTO_FIX_SHIFT))
#define CARTO_FIX_HALF  ((carto_fix)(1 << (CARTO_FIX_SHIFT - 1)))
#define CARTO_FIX_MASK  ((carto_fix)(CARTO_FIX_ONE - 1))

static inline carto_fix carto_fix_from_int(int32_t v) {
    return (carto_fix)(v << CARTO_FIX_SHIFT);
}

static inline int32_t carto_fix_to_int(carto_fix v) {
    return (int32_t)(v >> CARTO_FIX_SHIFT);
}

static inline int32_t carto_fix_round_int(carto_fix v) {
    return (int32_t)((v + CARTO_FIX_HALF) >> CARTO_FIX_SHIFT);
}

static inline carto_fix carto_fix_from_float(float f) {
    return (carto_fix)(f * (float)CARTO_FIX_ONE);
}

static inline float carto_fix_to_float(carto_fix v) {
    return (float)v / (float)CARTO_FIX_ONE;
}

static inline carto_fix carto_fix_mul(carto_fix a, carto_fix b) {
    return (carto_fix)(((int64_t)a * (int64_t)b) >> CARTO_FIX_SHIFT);
}

static inline carto_fix carto_fix_div(carto_fix a, carto_fix b) {
    if (b == 0) return 0;
    return (carto_fix)((((int64_t)a) << CARTO_FIX_SHIFT) / b);
}

static inline carto_fix carto_fix_floor(carto_fix v) {
    return v & ~CARTO_FIX_MASK;
}

static inline carto_fix carto_fix_ceil(carto_fix v) {
    return (v + CARTO_FIX_MASK) & ~CARTO_FIX_MASK;
}

static inline carto_fix carto_fix_min(carto_fix a, carto_fix b) {
    return a < b ? a : b;
}

static inline carto_fix carto_fix_max(carto_fix a, carto_fix b) {
    return a > b ? a : b;
}

#ifdef __cplusplus
}
#endif

#endif
