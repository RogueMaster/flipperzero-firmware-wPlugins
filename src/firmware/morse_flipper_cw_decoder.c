/*
 * Purpose: Decode keyed mark/space timings into text.
 * Owns: adaptive dit tracking, pending timing samples, and decoder output.
 * Depends on: cw.h and morse_flipper_cw_token.h.
 * Tests: tests/test_cw_decoder.c and tests/test_decoder_rf_integration.c.
 */

#include "morse_flipper_cw_decoder.h"
#include "cw.h"
#include "morse_flipper_cw_token.h"

#include <string.h>

static void decoder_emit(MorseFlipperCwDecoder* decoder, uint8_t ch) {
    if(!decoder || !ch) return;
    if(decoder->output_len + 1u >= sizeof(decoder->output)) return;
    decoder->output[decoder->output_len++] = (char)ch;
    decoder->output[decoder->output_len] = 0;
}

static void decoder_clear_symbol(MorseFlipperCwDecoder* decoder) {
    if(!decoder) return;
    decoder->symbol_code = 1u;
    decoder->symbol_count = 0u;
}

static void decoder_push_dit_sample(MorseFlipperCwDecoder* decoder, uint16_t ms) {
    uint16_t dit_ms;
    uint16_t limit;

    if(!decoder || !ms) return;
    if(decoder->fixed_timing) return;

    /* A first bounce must not replace the configured-speed seed. */
    dit_ms = decoder->dit_ms;
    limit = (uint16_t)(dit_ms / 4U);
    if(ms > dit_ms) {
        uint16_t step = (uint16_t)((ms - dit_ms) / 4U);
        decoder->dit_ms = (uint16_t)(dit_ms + (step > limit ? limit : step));
    } else {
        uint16_t step = (uint16_t)((dit_ms - ms) / 4U);
        decoder->dit_ms = (uint16_t)(dit_ms - (step > limit ? limit : step));
    }
    if(decoder->dit_sample_count != UINT8_MAX) decoder->dit_sample_count++;
}

static bool decoder_push_symbol(MorseFlipperCwDecoder* decoder, bool dash) {
    uint16_t bit;

    if(!decoder || decoder->symbol_count >= 9u) return false;

    bit = (uint16_t)(1u << decoder->symbol_count);
    if(dash)
        decoder->symbol_code |= bit;
    else
        decoder->symbol_code &= (uint16_t)~bit;

    decoder->symbol_count++;
    decoder->symbol_code |= (uint16_t)(1u << decoder->symbol_count);
    return true;
}

static uint8_t decoder_lookup(uint16_t code) {
    uint8_t i;
    static const uint8_t prosigns[] = {
        MORSE_FLIPPER_CW_TOKEN_SK,
        MORSE_FLIPPER_CW_TOKEN_BK,
        MORSE_FLIPPER_CW_TOKEN_CT_KA,
        MORSE_FLIPPER_CW_TOKEN_VE_SN,
        MORSE_FLIPPER_CW_TOKEN_AA,
    };

    if(code <= 1u || code == CW_INVALID) return 0;

    if(code == morse_flipper_cw_token_code(MORSE_FLIPPER_CW_TOKEN_SOS)) {
        return MORSE_FLIPPER_CW_TOKEN_SOS;
    }

    for(i = 0u; i < sizeof(prosigns) / sizeof(prosigns[0]); i++) {
        if(morse_flipper_cw_token_code(prosigns[i]) == code) return prosigns[i];
    }

    if(code > 0xFFu) return '#';
    for(i = 0u; i < sizeof(cw_ascii); i++)
        if(cw_ascii[i] == (uint8_t)code) return i;

    return '#';
}

static bool decoder_preview_extendable(uint16_t code, size_t count) {
    uint16_t bit;
    uint16_t next_code;
    uint8_t preview;
    uint8_t next;

    if(code <= 1u || count >= 9u) return false;

    preview = decoder_lookup(code);
    bit = (uint16_t)(1u << count);

    next_code = (uint16_t)(code & (uint16_t)~bit);
    next_code |= (uint16_t)(1u << (count + 1u));
    next = decoder_lookup(next_code);
    if(next != 0 && next != '#' && next != preview) return true;

    next_code = (uint16_t)(code | bit);
    next_code |= (uint16_t)(1u << (count + 1u));
    next = decoder_lookup(next_code);
    if(next != 0 && next != '#' && next != preview) return true;

    return false;
}

static void decoder_flush_symbol_buffer(MorseFlipperCwDecoder* decoder) {
    uint8_t letter;

    if(!decoder || !decoder->symbol_count) return;
    letter = decoder_lookup(decoder->symbol_code);
    decoder_emit(decoder, letter);
    decoder_clear_symbol(decoder);
}

static void decoder_process_mark(MorseFlipperCwDecoder* decoder, uint16_t ms) {
    uint32_t dit_min;
    uint32_t dit_max;
    uint32_t dah_min;
    uint32_t dah_max;

    if(!decoder || !ms) return;

    if(!decoder->dit_ms) return;

    dit_min = (decoder->dit_ms * 2u) / 3u;
    dit_max = decoder->dit_ms * 2u;
    dah_min = decoder->dit_ms * 2u;
    dah_max = decoder->dit_ms * 5u;

    if(ms >= dit_min && ms <= dit_max) {
        if(decoder_push_symbol(decoder, false)) {
            decoder_push_dit_sample(decoder, ms);
        }
    } else if(ms >= dah_min && ms <= dah_max) {
        decoder_push_symbol(decoder, true);
    }
}

static void decoder_process_space(MorseFlipperCwDecoder* decoder, uint16_t ms) {
    uint32_t letter_min;
    uint32_t word_min;
    uint32_t reset_min;

    if(!decoder || !ms) return;

    /* Spaces are boundaries: letter, word, or "give up and reset timing". */
    if(!decoder->dit_ms) return;

    letter_min = (decoder->dit_ms * 5u) / 2u;
    word_min = decoder->dit_ms * 6u;
    reset_min = decoder->dit_ms * 12u;

    if(ms >= letter_min && ms < word_min) {
        decoder_flush_symbol_buffer(decoder);
    } else if(ms >= word_min && ms < reset_min) {
        decoder_flush_symbol_buffer(decoder);
        decoder_emit(decoder, ' ');
    } else if(ms >= reset_min) {
        decoder_flush_symbol_buffer(decoder);
        decoder_emit(decoder, '|');
        decoder->timing_reset = true;
        decoder_clear_symbol(decoder);
    }
}

static void morse_flipper_cw_decoder_init_mode(
    MorseFlipperCwDecoder* decoder,
    uint16_t starting_dit_ms,
    bool fixed_timing) {
    if(!decoder) return;
    memset(decoder, 0, sizeof(*decoder));
    decoder->dit_ms = starting_dit_ms;
    decoder->fixed_timing = fixed_timing;
    decoder_clear_symbol(decoder);
}

void morse_flipper_cw_decoder_init(MorseFlipperCwDecoder* decoder, uint16_t starting_dit_ms) {
    morse_flipper_cw_decoder_init_mode(decoder, starting_dit_ms, false);
}

void morse_flipper_cw_decoder_init_fixed(MorseFlipperCwDecoder* decoder, uint16_t starting_dit_ms) {
    morse_flipper_cw_decoder_init_mode(decoder, starting_dit_ms, true);
}

void morse_flipper_cw_decoder_reset(MorseFlipperCwDecoder* decoder) {
    uint16_t starting_dit_ms;
    bool fixed_timing;

    if(!decoder) return;
    starting_dit_ms = decoder->dit_ms;
    fixed_timing = decoder->fixed_timing;
    memset(decoder, 0, sizeof(*decoder));
    decoder->dit_ms = starting_dit_ms;
    decoder->fixed_timing = fixed_timing;
    decoder_clear_symbol(decoder);
}

void morse_flipper_cw_decoder_feed_mark(MorseFlipperCwDecoder* decoder, uint16_t ms) {
    if(!decoder || !ms) return;
    decoder->timing_reset = false;
    decoder_process_mark(decoder, ms);
}

void morse_flipper_cw_decoder_feed_space(MorseFlipperCwDecoder* decoder, uint16_t ms) {
    if(!decoder || !ms) return;
    decoder->timing_reset = false;
    decoder_process_space(decoder, ms);
}

bool morse_flipper_cw_decoder_timing_ready(const MorseFlipperCwDecoder* decoder) {
    return decoder ? decoder->dit_ms != 0 : false;
}

uint16_t morse_flipper_cw_decoder_dit_ms(const MorseFlipperCwDecoder* decoder) {
    return decoder ? decoder->dit_ms : 0;
}

const char* morse_flipper_cw_decoder_output(const MorseFlipperCwDecoder* decoder) {
    return decoder ? decoder->output : "";
}

void morse_flipper_cw_decoder_clear_output(MorseFlipperCwDecoder* decoder) {
    if(!decoder) return;
    decoder->output_len = 0;
    decoder->output[0] = 0;
}

bool morse_flipper_cw_decoder_timing_reset(const MorseFlipperCwDecoder* decoder) {
    return decoder ? decoder->timing_reset : false;
}

uint8_t morse_flipper_cw_decoder_preview(const MorseFlipperCwDecoder* decoder) {
    if(!decoder || !decoder->symbol_count) return 0;
    return decoder_lookup(decoder->symbol_code);
}

bool morse_flipper_cw_decoder_preview_extendable(const MorseFlipperCwDecoder* decoder) {
    if(!decoder || !decoder->symbol_count) return false;
    return decoder_preview_extendable(decoder->symbol_code, decoder->symbol_count);
}
