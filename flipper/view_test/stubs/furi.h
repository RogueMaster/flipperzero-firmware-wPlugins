/* furi.h stub for running the screens on the host. */
#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define furi_check(x) assert(x)
#define UNUSED(x)     (void)(x)

/* One tick == one millisecond, as on the device (configTICK_RATE_HZ == 1000). */
static inline uint32_t furi_ms_to_ticks(uint32_t ms) {
    return ms;
}

/* The test decides what "now" is. */
extern uint32_t tpms_test_tick;

static inline uint32_t furi_get_tick(void) {
    return tpms_test_tick;
}

typedef void FuriMutex;
typedef void FuriMessageQueue;
typedef void FuriThread;
typedef void FuriString;
