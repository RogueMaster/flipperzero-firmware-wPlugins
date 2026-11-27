#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MfPassiveCodecS16 = 0,
    MfPassiveCodecU8 = 1,
    MfPassiveCodecMulaw = 2,
    MfPassiveCodecImaAdpcm = 3,
} MfPassiveCodec;

typedef struct {
    uint8_t codec;
    uint32_t samples_left;
    uint16_t byte_pos;
    int16_t ima_predictor;
    uint8_t ima_index;
    bool ima_high_nibble;
} MfPassiveCodecState;

bool mf_passive_codec_begin(
    MfPassiveCodecState* state,
    MfPassiveCodec codec,
    uint32_t logical_samples,
    int16_t ima_predictor,
    uint8_t ima_index);
size_t mf_passive_codec_decode(
    MfPassiveCodecState* state,
    const uint8_t* source,
    size_t source_len,
    int16_t* destination,
    size_t destination_cap,
    size_t* source_used);
bool mf_passive_codec_finished(const MfPassiveCodecState* state);
