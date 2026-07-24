#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "mf_passive_codec.h"

static unsigned checks;
#define CHECK(x) do { assert(x); checks++; } while(0)

static const int16_t ima_steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55,
    60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500,
    20350, 22385, 24623, 27086, 29794, 32767};
static const int8_t ima_index_delta[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

static int16_t mulaw_reference(uint8_t input) {
    uint8_t value = (uint8_t)~input;
    int32_t sample = ((int32_t)(value & 15U) << 3U) + 0x84;
    sample <<= (value >> 4U) & 7U;
    return (int16_t)((value & 0x80U) ? 0x84 - sample : sample - 0x84);
}

static int16_t ima_reference(int16_t predictor, uint8_t* index, uint8_t nibble) {
    int32_t step = ima_steps[*index];
    int32_t delta = step >> 3;
    int32_t next_index;
    int32_t sample;
    if(nibble & 1U) delta += step >> 2;
    if(nibble & 2U) delta += step >> 1;
    if(nibble & 4U) delta += step;
    sample = predictor + ((nibble & 8U) ? -delta : delta);
    if(sample < INT16_MIN) sample = INT16_MIN;
    if(sample > INT16_MAX) sample = INT16_MAX;
    next_index = (int32_t)*index + ima_index_delta[nibble];
    if(next_index < 0) next_index = 0;
    if(next_index > 88) next_index = 88;
    *index = (uint8_t)next_index;
    return (int16_t)sample;
}

int main(void) {
    MfPassiveCodecState state;
    int16_t output[8];
    size_t used;
    const uint8_t s16[] = {0x00, 0x80, 0x34, 0x12, 0xff, 0x7f};
    const uint8_t u8[] = {0, 128, 255};
    const uint8_t ima[] = {0x10, 0x7f};

    CHECK(!mf_passive_codec_begin(&state, MfPassiveCodecImaAdpcm, 1U, 0, 89U));
    CHECK(mf_passive_codec_begin(&state, MfPassiveCodecS16, 3U, 0, 0));
    CHECK(mf_passive_codec_decode(&state, s16, sizeof(s16), output, 8U, &used) == 3U);
    CHECK(used == 6U && output[0] == -32768 && output[1] == 0x1234 && output[2] == 32767);
    CHECK(mf_passive_codec_finished(&state));
    CHECK(mf_passive_codec_begin(&state, MfPassiveCodecU8, 3U, 0, 0));
    CHECK(mf_passive_codec_decode(&state, u8, sizeof(u8), output, 8U, &used) == 3U);
    CHECK(output[0] == -32768 && output[1] == 0 && output[2] == 32512);
    CHECK(mf_passive_codec_begin(&state, MfPassiveCodecMulaw, 256U, 0, 0));
    for(unsigned byte = 0U; byte < 256U; byte++) {
        uint8_t value = (uint8_t)byte;
        CHECK(mf_passive_codec_decode(&state, &value, 1U, output, 1U, &used) == 1U);
        CHECK(used == 1U);
        CHECK(output[0] == mulaw_reference(value));
    }
    CHECK(mf_passive_codec_finished(&state));
    CHECK(mf_passive_codec_begin(&state, MfPassiveCodecImaAdpcm, 4U, 0, 0));
    CHECK(mf_passive_codec_decode(&state, ima, sizeof(ima), output, 8U, &used) == 4U);
    CHECK(used == 2U && output[0] == 0 && output[1] == 1);
    CHECK(mf_passive_codec_begin(&state, MfPassiveCodecImaAdpcm, 2U, 0, 0));
    CHECK(mf_passive_codec_decode(&state, ima, 1U, output, 1U, &used) == 1U);
    CHECK(used == 0U);
    CHECK(mf_passive_codec_decode(&state, ima, 1U, output + 1U, 1U, &used) == 1U);
    CHECK(used == 1U && output[0] == 0 && output[1] == 1);
    for(uint8_t index = 0U; index <= 88U; index++) {
        for(uint8_t nibble = 0U; nibble < 16U; nibble++) {
            uint8_t expected_index = index;
            uint8_t value = nibble;
            int16_t expected = ima_reference(nibble & 8U ? INT16_MIN : INT16_MAX, &expected_index, nibble);
            CHECK(mf_passive_codec_begin(
                &state, MfPassiveCodecImaAdpcm, 1U, nibble & 8U ? INT16_MIN : INT16_MAX, index));
            CHECK(mf_passive_codec_decode(&state, &value, 1U, output, 1U, &used) == 1U);
            CHECK(output[0] == expected && state.ima_index == expected_index);
        }
    }
    printf("test_passive_codec: %u checks passed\n", checks);
    return 0;
}
