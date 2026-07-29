#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mf_rx_rng.h"

#define MF_CALLSIGN_MAX_LEN    6U
#define MF_CALLSIGN_PREFIX_MAX 3U

typedef enum {
    MfCallsignEntityUs = 0,
    MfCallsignEntityGermany,
    MfCallsignEntityItaly,
    MfCallsignEntityCanada,
    MfCallsignEntityRomania,
    MfCallsignEntityCount,
} MfCallsignEntity;

typedef struct {
    char text[MF_CALLSIGN_MAX_LEN + 1U];
    char prefix[MF_CALLSIGN_PREFIX_MAX + 1U];
    uint8_t text_len;
    uint8_t prefix_len;
    uint8_t entity;
} MfCallsign;

typedef struct {
    char last_prefix[MF_CALLSIGN_PREFIX_MAX + 1U];
    uint8_t last_prefix_len;
} MfCallsignGen;

void mf_callsign_gen_init(MfCallsignGen* gen);
uint8_t mf_callsign_pick_length(MfRxRng* rng);
bool mf_callsign_generate(MfCallsignGen* gen, MfRxRng* rng, uint8_t target_len, MfCallsign* out);
bool mf_callsign_valid(const MfCallsign* call, uint8_t target_len);
