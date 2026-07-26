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

static void feed_letter(MorseFlipperCwDecoder* decoder, const char* pattern, uint16_t dit_ms) {
    for(size_t i = 0U; pattern[i] != '\0'; i++) {
        morse_flipper_cw_decoder_feed_mark(
            decoder, pattern[i] == '-' ? (uint16_t)(dit_ms * 3U) : dit_ms);
        if(pattern[i + 1U] != '\0') morse_flipper_cw_decoder_feed_space(decoder, dit_ms);
    }
    morse_flipper_cw_decoder_feed_space(decoder, (uint16_t)(dit_ms * 3U));
}

static void test_adaptive_bounce_does_not_split_p(void) {
    MorseFlipperCwDecoder decoder;

    morse_flipper_cw_decoder_init(&decoder, 100U);
    morse_flipper_cw_decoder_feed_mark(&decoder, 50U);
    morse_flipper_cw_decoder_feed_space(&decoder, 30U);
    feed_letter(&decoder, ".--.", 100U);
    CHECK(strcmp(morse_flipper_cw_decoder_output(&decoder), "P") == 0);
    CHECK(morse_flipper_cw_decoder_dit_ms(&decoder) >= 90U);
}

static void test_adaptive_seed_converges_without_boundary_collapse(void) {
    MorseFlipperCwDecoder decoder;

    morse_flipper_cw_decoder_init(&decoder, 100U);
    feed_letter(&decoder, ".--.", 150U);
    feed_letter(&decoder, ".-", 150U);
    feed_letter(&decoder, ".-.", 150U);
    feed_letter(&decoder, "..", 150U);
    feed_letter(&decoder, "...", 150U);
    CHECK(strcmp(morse_flipper_cw_decoder_output(&decoder), "PARIS") == 0);
    CHECK(morse_flipper_cw_decoder_dit_ms(&decoder) > 125U);
}

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
    test_adaptive_bounce_does_not_split_p();
    test_adaptive_seed_converges_without_boundary_collapse();
    printf("test_cw_decoder_preview: %u checks passed\n", checks);
    return 0;
}
