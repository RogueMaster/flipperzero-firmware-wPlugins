#pragma once
#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

#define RAW_SAMPLES_NUM 2048U

typedef struct RawSamplesBuffer {
    FuriMutex* mutex;
    struct {
        uint16_t level : 1;
        uint16_t dur : 15;
    } samples[RAW_SAMPLES_NUM];
    uint32_t idx;
    uint32_t total;
    uint32_t short_pulse_dur;
} RawSamplesBuffer;

RawSamplesBuffer* raw_samples_alloc(void);
void raw_samples_reset(RawSamplesBuffer* s);
void raw_samples_center(RawSamplesBuffer* s, uint32_t offset);
void raw_samples_add(RawSamplesBuffer* s, bool level, uint32_t dur);
void raw_samples_add_or_update(RawSamplesBuffer* s, bool level, uint32_t dur);
void raw_samples_get(RawSamplesBuffer* s, uint32_t idx, bool* level, uint32_t* dur);
/* Only for a private snapshot that is not modified concurrently. */
void raw_samples_get_unlocked(
    const RawSamplesBuffer* s,
    uint32_t idx,
    bool* level,
    uint32_t* dur);
void raw_samples_copy(RawSamplesBuffer* dst, RawSamplesBuffer* src);
void raw_samples_free(RawSamplesBuffer* s);
