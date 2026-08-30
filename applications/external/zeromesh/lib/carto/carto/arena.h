#ifndef CARTO_ARENA_H
#define CARTO_ARENA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *base;
    size_t   size;
    size_t   used;
    size_t   peak;
} carto_arena;

static inline void carto_arena_init(carto_arena *a, void *buffer, size_t size) {
    a->base = (uint8_t *)buffer;
    a->size = size;
    a->used = 0;
    a->peak = 0;
}

static inline void *carto_arena_alloc(carto_arena *a, size_t bytes, size_t align) {
    size_t mask = align - 1;
    size_t off  = (a->used + mask) & ~mask;
    if (off + bytes > a->size) return NULL;
    a->used = off + bytes;
    if (a->used > a->peak) a->peak = a->used;
    return a->base + off;
}

static inline void carto_arena_reset(carto_arena *a) {
    a->used = 0;
}

static inline size_t carto_arena_used(const carto_arena *a)      { return a->used; }
static inline size_t carto_arena_remaining(const carto_arena *a) { return a->size - a->used; }
static inline size_t carto_arena_peak(const carto_arena *a)      { return a->peak; }

#ifdef __cplusplus
}
#endif

#endif
