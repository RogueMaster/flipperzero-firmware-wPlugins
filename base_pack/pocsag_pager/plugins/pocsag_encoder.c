#include "pocsag_encoder.h"

#include <string.h>

#define POCSAG_BCH_POLY         0x769U
#define POCSAG_BATCH_DATA_WORDS 16U

uint32_t pocsag_bch_encode(uint32_t data_top21_shifted) {
    uint32_t data = data_top21_shifted & 0xFFFFF800U;
    uint32_t rem = data;

    for(uint8_t i = 0; i < 21; i++) {
        if(rem & 0x80000000U) {
            rem ^= (POCSAG_BCH_POLY << 21);
        }
        rem <<= 1;
    }

    uint32_t cw = data | (rem >> 21);

    // Even parity over bits [31:1] stored in bit 0
    uint32_t p = cw;
    p ^= p >> 16;
    p ^= p >> 8;
    p ^= p >> 4;
    p ^= p >> 2;
    p ^= p >> 1;
    return cw | (p & 1U);
}

static bool pocsag_push_word(
    uint32_t* out,
    size_t max_words,
    size_t* count,
    uint8_t* slot,
    uint32_t word) {
    if(*slot == POCSAG_BATCH_DATA_WORDS) {
        if(*count >= max_words) return false;
        out[(*count)++] = POCSAG_FRAME_SYNC_WORD;
        *slot = 0;
    }
    if(*count >= max_words) return false;
    out[(*count)++] = word;
    (*slot)++;
    return true;
}

static int8_t pocsag_numeric_char_to_bcd(char c) {
    if(c >= '0' && c <= '9') return (int8_t)(c - '0');
    switch(c) {
    case '*':
    case 'R':
    case 'r':
        return 0xA;
    case 'U':
    case 'u':
        return 0xB;
    case ' ':
        return 0xC;
    case '-':
        return 0xD;
    case ')':
    case ']':
        return 0xE;
    case '(':
    case '[':
        return 0xF;
    default:
        return -1;
    }
}

static uint8_t pocsag_rev4(uint8_t v) {
    return (uint8_t)(((v & 0x1U) << 3) | ((v & 0x2U) << 1) | ((v & 0x4U) >> 1) |
                     ((v & 0x8U) >> 3));
}

size_t pocsag_encode_codewords(
    uint32_t ric,
    PocsagMsgType msg_type,
    const char* message,
    uint32_t* out_codewords,
    size_t max_codewords) {
    if(!out_codewords || max_codewords < 17U || ric > POCSAG_RIC_MAX) {
        return 0;
    }

    size_t count = 0;
    uint8_t slot = 0;

    // Initial Frame Sync Codeword for batch 0
    out_codewords[count++] = POCSAG_FRAME_SYNC_WORD;

    // Address codeword must sit in the frame matching the low 3 bits of RIC
    uint8_t addr_slot = (uint8_t)((ric & 0x7U) * 2U);
    while(slot < addr_slot) {
        if(!pocsag_push_word(out_codewords, max_codewords, &count, &slot, POCSAG_IDLE_WORD)) {
            return 0;
        }
    }

    uint32_t addr_top21 = ((ric >> 3) << 13) | (((uint32_t)msg_type & 0x3U) << 11);
    uint32_t addr_cw = pocsag_bch_encode(addr_top21);
    if(!pocsag_push_word(out_codewords, max_codewords, &count, &slot, addr_cw)) {
        return 0;
    }

    const bool has_text = (message != NULL && message[0] != '\0');

    if(msg_type == PocsagMsgTypeNumeric && has_text) {
        uint32_t msg_bits = 0;
        uint8_t nibbles = 0;
        for(size_t i = 0; message[i] != '\0' && i < POCSAG_MAX_MSG_LEN; i++) {
            int8_t bcd = pocsag_numeric_char_to_bcd(message[i]);
            if(bcd < 0) continue;
            msg_bits = (msg_bits << 4) | pocsag_rev4((uint8_t)bcd);
            nibbles++;
            if(nibbles == 5U) {
                uint32_t cw = pocsag_bch_encode(0x80000000U | (msg_bits << 11));
                if(!pocsag_push_word(out_codewords, max_codewords, &count, &slot, cw)) {
                    return 0;
                }
                msg_bits = 0;
                nibbles = 0;
            }
        }
        if(nibbles > 0U) {
            // Pad unused trailing BCD nibbles with Space (0xC -> bit-reversed 0x3)
            while(nibbles < 5U) {
                msg_bits = (msg_bits << 4) | pocsag_rev4(0xCU);
                nibbles++;
            }
            uint32_t cw = pocsag_bch_encode(0x80000000U | (msg_bits << 11));
            if(!pocsag_push_word(out_codewords, max_codewords, &count, &slot, cw)) {
                return 0;
            }
        }
    } else if(
        (msg_type == PocsagMsgTypeAlpha || msg_type == PocsagMsgTypeAlert2) && has_text) {
        uint32_t msg_bits = 0;
        uint8_t bit_count = 0;
        for(size_t i = 0; message[i] != '\0' && i < POCSAG_MAX_MSG_LEN; i++) {
            uint8_t ch = (uint8_t)message[i] & 0x7FU;
            for(uint8_t b = 0; b < 7U; b++) {
                msg_bits = (msg_bits << 1) | ((ch >> b) & 1U);
                bit_count++;
                if(bit_count == 20U) {
                    uint32_t cw = pocsag_bch_encode(0x80000000U | (msg_bits << 11));
                    if(!pocsag_push_word(out_codewords, max_codewords, &count, &slot, cw)) {
                        return 0;
                    }
                    msg_bits = 0;
                    bit_count = 0;
                }
            }
        }
        if(bit_count > 0U) {
            // Pad remaining bits in the final codeword with zeros (NUL)
            msg_bits <<= (20U - bit_count);
            uint32_t cw = pocsag_bch_encode(0x80000000U | (msg_bits << 11));
            if(!pocsag_push_word(out_codewords, max_codewords, &count, &slot, cw)) {
                return 0;
            }
        }
    }

    // Complete current batch with IDLE codewords. If the message ended on slot 15, emit a
    // trailing idle batch so receivers always see an explicit IDLE terminator.
    if(slot == POCSAG_BATCH_DATA_WORDS) {
        if(!pocsag_push_word(out_codewords, max_codewords, &count, &slot, POCSAG_IDLE_WORD)) {
            return 0;
        }
    }
    while(slot < POCSAG_BATCH_DATA_WORDS) {
        if(!pocsag_push_word(out_codewords, max_codewords, &count, &slot, POCSAG_IDLE_WORD)) {
            return 0;
        }
    }

    return count;
}

static inline bool pocsag_encoder_bit_level(const PocsagEncoder* enc, uint32_t bit_idx) {
    bool bit;
    if(bit_idx < enc->preamble_bits) {
        // Normal polarity: 0,1,0,1,...,0 (577 bits, starts with 0 -> HIGH so async TX middleware
        // keeps every bit; ends with 1,0 before FSC).
        // Inverted polarity: 1,0,1,0,...,0 (576 bits, starts with 1 -> HIGH; ends with 1,0).
        bit = enc->inverted ? ((bit_idx & 1U) == 0U) : ((bit_idx & 1U) != 0U);
    } else {
        uint32_t d = bit_idx - enc->preamble_bits;
        bit = ((enc->codewords[d >> 5] >> (31U - (d & 31U))) & 1U) != 0U;
    }
    return enc->inverted ? bit : !bit;
}

bool pocsag_encoder_init(
    PocsagEncoder* enc,
    uint32_t ric,
    PocsagMsgType msg_type,
    const char* message,
    PocsagBaudRate baud,
    bool inverted) {
    if(!enc) return false;
    if(baud != PocsagBaud512 && baud != PocsagBaud1200 && baud != PocsagBaud2400) {
        return false;
    }

    memset(enc, 0, sizeof(PocsagEncoder));
    enc->codeword_count = pocsag_encode_codewords(
        ric, msg_type, message, enc->codewords, POCSAG_MAX_CODEWORDS);
    if(enc->codeword_count == 0U) return false;

    enc->baud = (uint32_t)baud;
    enc->inverted = inverted;
    enc->preamble_bits = inverted ? POCSAG_PREAMBLE_BITS : (POCSAG_PREAMBLE_BITS + 1U);
    enc->total_bits = enc->preamble_bits + (uint32_t)(enc->codeword_count * 32U);
    enc->bit_idx = 0;
    enc->last_level = true;
    enc->done = false;
    return true;
}

uint32_t pocsag_encoder_get_duration_ms(const PocsagEncoder* enc) {
    if(!enc || enc->baud == 0U) return 0U;
    return (uint32_t)(((uint64_t)enc->total_bits * 1000ULL + enc->baud - 1ULL) / enc->baud);
}

bool pocsag_encoder_next_pulse(PocsagEncoder* enc, bool* out_level, uint32_t* out_duration_us) {
    if(!enc || enc->done) return false;

    if(enc->bit_idx >= enc->total_bits) {
        enc->done = true;
        // Flipper's async TX middleware discards a trailing LOW run upon level_duration_reset().
        // Emit a 1-bit HIGH flush pulse whenever the final real run was LOW so the last
        // codeword's trailing bits are always transmitted in full.
        if(!enc->last_level) {
            *out_level = true;
            *out_duration_us = (1000000U + enc->baud / 2U) / enc->baud;
            return true;
        }
        return false;
    }

    uint32_t start = enc->bit_idx;
    bool level = pocsag_encoder_bit_level(enc, start);
    uint32_t end = start + 1U;
    while(end < enc->total_bits && pocsag_encoder_bit_level(enc, end) == level) {
        end++;
    }
    enc->bit_idx = end;
    enc->last_level = level;

    // Exact cumulative edge timestamps -> zero clock drift at 512, 1200, and 2400 baud
    uint64_t t0 = ((uint64_t)start * 1000000ULL + enc->baud / 2ULL) / enc->baud;
    uint64_t t1 = ((uint64_t)end * 1000000ULL + enc->baud / 2ULL) / enc->baud;

    *out_level = level;
    *out_duration_us = (uint32_t)(t1 - t0);
    return true;
}

#ifndef POCSAG_ENCODER_HOST_TEST
LevelDuration pocsag_encoder_yield(void* context) {
    PocsagEncoder* enc = context;
    bool level = false;
    uint32_t duration_us = 0;
    if(!pocsag_encoder_next_pulse(enc, &level, &duration_us)) {
        return level_duration_reset();
    }
    return level_duration_make(level, duration_us);
}
#endif
