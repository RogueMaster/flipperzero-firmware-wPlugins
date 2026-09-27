#pragma once
/*
 * Just enough of furi.h to compile the pure helpers on a laptop.
 *
 * The point is not to emulate the firmware. It is that tracker_db.c holds the
 * heuristic that decides whether to tell somebody they are being followed, and
 * that decision turns on TIME - so it needs to be testable with time under the
 * test's control rather than only ever observable by standing in a room for
 * ten minutes holding a Flipper.
 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define furi_assert(x) assert(x)
#define furi_check(x)  assert(x)
#define UNUSED(x)      (void)(x)

/* The tests drive this directly, which is the whole reason for the shim. */
extern uint32_t gt_test_tick;
static inline uint32_t furi_get_tick(void) {
    return gt_test_tick;
}
static inline uint32_t furi_ms_to_ticks(uint32_t ms) {
    return ms;
}
static inline uint32_t furi_kernel_get_tick_frequency(void) {
    return 1000;
}
static inline void furi_delay_tick(uint32_t t) {
    (void)t;
}
static inline void furi_delay_ms(uint32_t t) {
    (void)t;
}

/* Single-threaded on the host, so the mutex only has to exist. */
typedef struct FuriMutex FuriMutex;
typedef enum { FuriMutexTypeNormal, FuriMutexTypeRecursive } FuriMutexType;
#define FuriWaitForever 0xFFFFFFFFU

static inline FuriMutex* furi_mutex_alloc(FuriMutexType t) {
    (void)t;
    return (FuriMutex*)malloc(1);
}
static inline void furi_mutex_free(FuriMutex* m) {
    free(m);
}
static inline int furi_mutex_acquire(FuriMutex* m, uint32_t to) {
    (void)m;
    (void)to;
    return 0;
}
static inline int furi_mutex_release(FuriMutex* m) {
    (void)m;
    return 0;
}
