#include "mf_passive_codec.h"

#include <limits.h>
#include <string.h>

static const int16_t mf_ima_steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55,
    60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500,
    20350, 22385, 24623, 27086, 29794, 32767};
static const int8_t mf_ima_index_delta[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

static int16_t mf_mulaw_decode(uint8_t input) {
    uint8_t value = (uint8_t)~input;
    int32_t sample = ((int32_t)(value & 0x0fU) << 3) + 0x84;
    sample <<= (value >> 4) & 0x07U;
    return (int16_t)((value & 0x80U) ? (0x84 - sample) : (sample - 0x84));
}

bool mf_passive_codec_begin(
    MfPassiveCodecState* state,
    MfPassiveCodec codec,
    uint32_t logical_samples,
    int16_t ima_predictor,
    uint8_t ima_index) {
    if(state == NULL || logical_samples == 0U || codec > MfPassiveCodecImaAdpcm ||
       (codec == MfPassiveCodecImaAdpcm && ima_index > 88U))
        return false;
    *state = (MfPassiveCodecState){
        .codec = codec,
        .samples_left = logical_samples,
        .ima_predictor = ima_predictor,
        .ima_index = ima_index,
    };
    return true;
}

static int16_t mf_ima_decode(MfPassiveCodecState* state, uint8_t nibble) {
    int32_t step = mf_ima_steps[state->ima_index];
    int32_t delta = step >> 3;
    int32_t predictor;
    if(nibble & 1U) delta += step >> 2;
    if(nibble & 2U) delta += step >> 1;
    if(nibble & 4U) delta += step;
    predictor = state->ima_predictor + ((nibble & 8U) ? -delta : delta);
    if(predictor > INT16_MAX) predictor = INT16_MAX;
    if(predictor < INT16_MIN) predictor = INT16_MIN;
    state->ima_predictor = (int16_t)predictor;
    int32_t index = (int32_t)state->ima_index + mf_ima_index_delta[nibble & 15U];
    if(index < 0) index = 0;
    if(index > 88) index = 88;
    state->ima_index = (uint8_t)index;
    return state->ima_predictor;
}

size_t mf_passive_codec_decode(
    MfPassiveCodecState* state,
    const uint8_t* source,
    size_t source_len,
    int16_t* destination,
    size_t destination_cap,
    size_t* source_used) {
    size_t produced = 0U;
    size_t used = 0U;
    if(source_used != NULL) *source_used = 0U;
    if(state == NULL || source == NULL || destination == NULL) return 0U;
    while(produced < destination_cap && state->samples_left != 0U) {
        if(state->codec == MfPassiveCodecS16) {
            if(used + 2U > source_len) break;
            destination[produced] = (int16_t)((uint16_t)source[used] | ((uint16_t)source[used + 1U] << 8));
            used += 2U;
        } else if(state->codec == MfPassiveCodecU8) {
            if(used >= source_len) break;
            destination[produced] = (int16_t)(((int32_t)source[used++] - 128) * 256);
        } else if(state->codec == MfPassiveCodecMulaw) {
            if(used >= source_len) break;
            destination[produced] = mf_mulaw_decode(source[used++]);
        } else {
            uint8_t nibble;
            if(used >= source_len) break;
            nibble = state->ima_high_nibble ? source[used++] >> 4 : source[used] & 15U;
            state->ima_high_nibble = !state->ima_high_nibble;
            destination[produced] = mf_ima_decode(state, nibble);
        }
        produced++;
        state->samples_left--;
    }
    if(source_used != NULL) *source_used = used;
    return produced;
}

bool mf_passive_codec_finished(const MfPassiveCodecState* state) {
    return state != NULL && state->samples_left == 0U;
}
