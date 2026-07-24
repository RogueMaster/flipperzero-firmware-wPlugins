#include <assert.h>
#include <stdio.h>

#include "mf_passive_codec.h"

static unsigned checks;
#define CHECK(x) do { assert(x); checks++; } while(0)

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
    }
    CHECK(mf_passive_codec_finished(&state));
    CHECK(mf_passive_codec_begin(&state, MfPassiveCodecImaAdpcm, 4U, 0, 0));
    CHECK(mf_passive_codec_decode(&state, ima, sizeof(ima), output, 8U, &used) == 4U);
    CHECK(used == 2U && output[0] == 0 && output[1] == 1);
    printf("test_passive_codec: %u checks passed\n", checks);
    return 0;
}
