#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MF_PASSIVE_PCM_RING_SAMPLES 1024U

typedef struct MfPassivePcmPipe {
    int16_t samples[MF_PASSIVE_PCM_RING_SAMPLES];
    volatile uint16_t read_pos;
    volatile uint16_t write_pos;
    volatile bool eof;
    volatile bool drained;
    volatile uint32_t underruns;
} MfPassivePcmPipe;
