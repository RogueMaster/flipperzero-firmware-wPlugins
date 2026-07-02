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
    size_t i;
    uint32_t total;

    if(!decoder || !ms) return;

    /* Rolling dit estimate: small, cheap, and good enough for a human fist. */
    if(decoder->dit_sample_count <
       sizeof(decoder->dit_samples) / sizeof(decoder->dit_samples[0])) {
        decoder->dit_samples[decoder->dit_sample_count++] = ms;
    } else {
        for(i = 1; i < decoder->dit_sample_count; i++) {
            decoder->dit_samples[i - 1] = decoder->dit_samples[i];
        }
        decoder->dit_samples[decoder->dit_sample_count - 1u] = ms;
    }

    total = 0;
    for(i = 0; i < decoder->dit_sample_count; i++)
        total += decoder->dit_samples[i];
    decoder->dit_ms = (uint16_t)(total / decoder->dit_sample_count);
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

static bool decoder_guess_timing(MorseFlipperCwDecoder* decoder) {
    uint16_t marks[32];
    size_t mark_count;
    size_t i;
    size_t j;
    size_t split_idx;
    uint16_t tmp;
    uint16_t largest_gap;
    uint32_t lower_total;

    /*
     * Before timing is known, pending samples store marks as positive and spaces
     * as negative. Marks should form two rough clusters; the lower cluster is dit.
     */
    if(!decoder) return false;

    mark_count = 0;
    for(i = 0; i < decoder->pending_count && mark_count < sizeof(marks) / sizeof(marks[0]); i++) {
        if(decoder->pending_samples[i] > 0)
            marks[mark_count++] = (uint16_t)decoder->pending_samples[i];
    }
    if(mark_count < 8) return false;

    for(i = 0; i < mark_count; i++) {
        for(j = i + 1; j < mark_count; j++) {
            if(marks[j] < marks[i]) {
                tmp = marks[i];
                marks[i] = marks[j];
                marks[j] = tmp;
            }
        }
    }

    split_idx = 0;
    largest_gap = 0;
    for(i = 1; i < mark_count; i++) {
        if((uint16_t)(marks[i] - marks[i - 1u]) > largest_gap) {
            largest_gap = (uint16_t)(marks[i] - marks[i - 1u]);
            split_idx = i;
        }
    }
    if(!split_idx || marks[split_idx - 1u] == 0) return false;
    if(marks[split_idx] / marks[split_idx - 1u] < 2u) return false;

    lower_total = 0;
    for(i = 0; i < split_idx; i++)
        lower_total += marks[i];
    decoder->dit_ms = (uint16_t)(lower_total / split_idx);
    decoder->dit_sample_count = 0;
    for(i = 0; i < split_idx && i < sizeof(decoder->dit_samples) / sizeof(decoder->dit_samples[0]);
        i++) {
        decoder->dit_samples[decoder->dit_sample_count++] = marks[i];
    }
    return true;
}

static void decoder_process_mark(MorseFlipperCwDecoder* decoder, uint16_t ms) {
    uint32_t dit_min;
    uint32_t dit_max;
    uint32_t dah_min;
    uint32_t dah_max;

    if(!decoder || !ms) return;

    /* No dit yet: bank samples until the cluster split is plausible. */
    if(!decoder->dit_ms) {
        if(decoder->pending_count <
           sizeof(decoder->pending_samples) / sizeof(decoder->pending_samples[0])) {
            decoder->pending_samples[decoder->pending_count++] = (int16_t)ms;
        }
        return;
    }

    dit_min = decoder->dit_ms / 2u;
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
    if(!decoder->dit_ms) {
        if(decoder->pending_count <
           sizeof(decoder->pending_samples) / sizeof(decoder->pending_samples[0])) {
            decoder->pending_samples[decoder->pending_count++] = -(int16_t)ms;
        }
        return;
    }

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
        decoder->pending_count = 0;
    }
}

static void decoder_replay_pending(MorseFlipperCwDecoder* decoder) {
    int16_t samples[64];
    size_t count;
    size_t i;

    /* Once a dit is guessed, replay old edges through the normal path. No special cases. */
    if(!decoder) return;
    count = decoder->pending_count;
    if(count > sizeof(samples) / sizeof(samples[0])) count = sizeof(samples) / sizeof(samples[0]);
    for(i = 0; i < count; i++)
        samples[i] = decoder->pending_samples[i];
    decoder->pending_count = 0;

    for(i = 0; i < count; i++) {
        if(samples[i] > 0)
            decoder_process_mark(decoder, (uint16_t)samples[i]);
        else
            decoder_process_space(decoder, (uint16_t)(-samples[i]));
    }
}

void morse_flipper_cw_decoder_init(MorseFlipperCwDecoder* decoder, uint16_t starting_dit_ms) {
    if(!decoder) return;
    memset(decoder, 0, sizeof(*decoder));
    decoder->dit_ms = starting_dit_ms;
    decoder_clear_symbol(decoder);
}

void morse_flipper_cw_decoder_reset(MorseFlipperCwDecoder* decoder) {
    uint16_t starting_dit_ms;

    if(!decoder) return;
    starting_dit_ms = decoder->dit_ms;
    memset(decoder, 0, sizeof(*decoder));
    decoder->dit_ms = starting_dit_ms;
    decoder_clear_symbol(decoder);
}

void morse_flipper_cw_decoder_feed_mark(MorseFlipperCwDecoder* decoder, uint16_t ms) {
    if(!decoder || !ms) return;
    decoder->timing_reset = false;
    decoder_process_mark(decoder, ms);
    if(!decoder->dit_ms && decoder_guess_timing(decoder)) {
        decoder_replay_pending(decoder);
    }
}

void morse_flipper_cw_decoder_feed_space(MorseFlipperCwDecoder* decoder, uint16_t ms) {
    if(!decoder || !ms) return;
    decoder->timing_reset = false;
    decoder_process_space(decoder, ms);
    if(!decoder->dit_ms && decoder_guess_timing(decoder)) {
        decoder_replay_pending(decoder);
    }
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
