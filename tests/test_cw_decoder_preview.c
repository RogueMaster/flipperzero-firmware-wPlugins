#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "morse_flipper_cw_decoder.h"

static unsigned checks;

#define CHECK(value)   \
    do {               \
        assert(value); \
        checks++;      \
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

static void test_adaptive_fast_fist_recovers_dit_and_dah_leads(void) {
    MorseFlipperCwDecoder decoder;

    morse_flipper_cw_decoder_init(&decoder, 100U);
    morse_flipper_cw_decoder_feed_mark(&decoder, 55U);
    CHECK(morse_flipper_cw_decoder_preview(&decoder) == 'E');
    morse_flipper_cw_decoder_reset(&decoder);
    feed_letter(&decoder, ".--.", 55U);
    feed_letter(&decoder, ".--.", 55U);
    CHECK(strcmp(morse_flipper_cw_decoder_output(&decoder), "PP") == 0);

    morse_flipper_cw_decoder_init(&decoder, 100U);
    feed_letter(&decoder, "-...", 55U);
    CHECK(strcmp(morse_flipper_cw_decoder_output(&decoder), "B") == 0);

    morse_flipper_cw_decoder_init(&decoder, 100U);
    feed_letter(&decoder, ".--.", 65U);
    CHECK(strcmp(morse_flipper_cw_decoder_output(&decoder), "P") == 0);
}

static void test_adaptive_bounce_sequence_is_ignored(void) {
    MorseFlipperCwDecoder decoder;

    morse_flipper_cw_decoder_init(&decoder, 100U);
    morse_flipper_cw_decoder_feed_mark(&decoder, 50U);
    morse_flipper_cw_decoder_feed_space(&decoder, 15U);
    morse_flipper_cw_decoder_feed_mark(&decoder, 50U);
    morse_flipper_cw_decoder_feed_space(&decoder, 30U);
    feed_letter(&decoder, ".--.", 100U);
    CHECK(strcmp(morse_flipper_cw_decoder_output(&decoder), "P") == 0);
}

static void test_zero_seed_uses_safe_default(void) {
    MorseFlipperCwDecoder decoder;

    morse_flipper_cw_decoder_init(&decoder, 0U);
    CHECK(morse_flipper_cw_decoder_dit_ms(&decoder) == 100U);
}

static void test_rx_callsigns_can_use_a_two_dit_letter_boundary(void) {
    MorseFlipperCwDecoder decoder;

    morse_flipper_cw_decoder_init_fixed(&decoder, 100U);
    morse_flipper_cw_decoder_feed_mark(&decoder, 100U);
    morse_flipper_cw_decoder_feed_space(&decoder, 200U);
    CHECK(morse_flipper_cw_decoder_output(&decoder)[0] == '\0');
    morse_flipper_cw_decoder_feed_space_with_letter_gap(&decoder, 200U, 200U);
    CHECK(strcmp(morse_flipper_cw_decoder_output(&decoder), "E") == 0);
}

static void test_rx_boundary_keeps_a_normal_irregular_character_intact(void) {
    MorseFlipperCwDecoder decoder;

    morse_flipper_cw_decoder_init_fixed(&decoder, 100U);
    morse_flipper_cw_decoder_feed_mark(&decoder, 100U);
    morse_flipper_cw_decoder_feed_space(&decoder, 150U);
    morse_flipper_cw_decoder_feed_mark(&decoder, 300U);
    morse_flipper_cw_decoder_feed_space(&decoder, 125U);
    morse_flipper_cw_decoder_feed_mark(&decoder, 300U);
    morse_flipper_cw_decoder_feed_space(&decoder, 150U);
    morse_flipper_cw_decoder_feed_mark(&decoder, 100U);
    morse_flipper_cw_decoder_feed_space_with_letter_gap(&decoder, 200U, 200U);
    CHECK(strcmp(morse_flipper_cw_decoder_output(&decoder), "P") == 0);
}

int main(void) {
    static const char expected[] = "EISH5";
    MorseFlipperCwDecoder decoder;

    morse_flipper_cw_decoder_init_fixed(&decoder, 100U);
    for(size_t i = 0U; i < strlen(expected); i++) {
        morse_flipper_cw_decoder_feed_mark(&decoder, 100U);
        CHECK(morse_flipper_cw_decoder_preview(&decoder) == (uint8_t)expected[i]);
        if(i + 1U < strlen(expected)) morse_flipper_cw_decoder_feed_space(&decoder, 100U);
    }

    morse_flipper_cw_decoder_feed_space(&decoder, 300U);
    CHECK(strcmp(morse_flipper_cw_decoder_output(&decoder), "5") == 0);
    test_adaptive_bounce_does_not_split_p();
    test_adaptive_seed_converges_without_boundary_collapse();
    test_adaptive_fast_fist_recovers_dit_and_dah_leads();
    test_adaptive_bounce_sequence_is_ignored();
    test_zero_seed_uses_safe_default();
    test_rx_callsigns_can_use_a_two_dit_letter_boundary();
    test_rx_boundary_keeps_a_normal_irregular_character_intact();
    printf("test_cw_decoder_preview: %u checks passed\n", checks);
    return 0;
}
