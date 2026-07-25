#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "morse_flipper_cw_decoder.h"

static unsigned checks;

#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)

int main(void) {
    static const char expected[] = "EISH5";
    MorseFlipperCwDecoder decoder;

    morse_flipper_cw_decoder_init_fixed(&decoder, 100U);
    for(size_t i = 0U; i < strlen(expected); i++) {
        morse_flipper_cw_decoder_feed_mark(&decoder, 100U);
        CHECK(morse_flipper_cw_decoder_preview(&decoder) == (uint8_t)expected[i]);
        if(i + 1U < strlen(expected))
            morse_flipper_cw_decoder_feed_space(&decoder, 100U);
    }

    morse_flipper_cw_decoder_feed_space(&decoder, 300U);
    CHECK(strcmp(morse_flipper_cw_decoder_output(&decoder), "5") == 0);
    printf("test_cw_decoder_preview: %u checks passed\n", checks);
    return 0;
}
