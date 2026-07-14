/*
 * Purpose: Publish the timing-based CW decoder state and API.
 * Owns: decoder buffers, dit estimates, and text output limits.
 * Depends on: host-safe integer types only.
 * Tests: tests/test_cw_decoder.c and tests/test_decoder_rf_integration.c.
 */

#ifndef yo3gnd_cw_decoder_2288
#define yo3gnd_cw_decoder_2288

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t dit_ms;
    uint16_t dit_samples[8];
    size_t dit_sample_count;
    int16_t pending_samples[64];
    size_t pending_count;
    uint16_t symbol_code;
    size_t symbol_count;
    char output[96];
    size_t output_len;
    bool timing_reset;
    bool fixed_timing;
} MorseFlipperCwDecoder;

void morse_flipper_cw_decoder_init(MorseFlipperCwDecoder* decoder, uint16_t starting_dit_ms);
void morse_flipper_cw_decoder_init_fixed(MorseFlipperCwDecoder* decoder, uint16_t starting_dit_ms);
void morse_flipper_cw_decoder_reset(MorseFlipperCwDecoder* decoder);
void morse_flipper_cw_decoder_feed_mark(MorseFlipperCwDecoder* decoder, uint16_t ms);
void morse_flipper_cw_decoder_feed_space(MorseFlipperCwDecoder* decoder, uint16_t ms);
bool morse_flipper_cw_decoder_timing_ready(const MorseFlipperCwDecoder* decoder);
uint16_t morse_flipper_cw_decoder_dit_ms(const MorseFlipperCwDecoder* decoder);
const char* morse_flipper_cw_decoder_output(const MorseFlipperCwDecoder* decoder);
void morse_flipper_cw_decoder_clear_output(MorseFlipperCwDecoder* decoder);
bool morse_flipper_cw_decoder_timing_reset(const MorseFlipperCwDecoder* decoder);
uint8_t morse_flipper_cw_decoder_preview(const MorseFlipperCwDecoder* decoder);
bool morse_flipper_cw_decoder_preview_extendable(const MorseFlipperCwDecoder* decoder);

#endif
