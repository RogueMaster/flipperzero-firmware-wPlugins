/* nfc_fuzzer_profiles.c — NFC card type profiles and UID mutation strategies.
 * Defines fuzz parameters (UID length, SAK, ATQA) for each supported card type. */

#include "nfc_fuzzer_profiles.h"

#define PROFILES_TAG "NfcFuzzerProfiles"

/* ─────────────────────────────────────────────────
 * Internal PRNG (xorshift32) for reproducible fuzzing
 * ───────────────────────────────────────────────── */

static uint32_t prng_state = 0x12345678;

static void prng_seed(uint32_t seed) {
    prng_state = seed ? seed : 0x12345678;
}

static uint32_t prng_next(void) {
    uint32_t x = prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;
    return x;
}

static uint8_t prng_byte(void) {
    return (uint8_t)(prng_next() & 0xFF);
}

/* ─────────────────────────────────────────────────
 * Boundary values used across profiles
 * ───────────────────────────────────────────────── */

static const uint8_t boundary_bytes[] = {
    0x00,
    0x01,
    0x7E,
    0x7F,
    0x80,
    0x81,
    0xFE,
    0xFF,
};
#define BOUNDARY_BYTE_COUNT (sizeof(boundary_bytes) / sizeof(boundary_bytes[0]))

/* ─────────────────────────────────────────────────
 * Helper: fill test case with random bytes
 * ───────────────────────────────────────────────── */

static void fill_random(uint8_t* buf, uint8_t len) {
    for(uint8_t i = 0; i < len; i++) {
        buf[i] = prng_byte();
    }
}

/* ─────────────────────────────────────────────────
 * Helper: apply bitflip mutation to a baseline
 * ───────────────────────────────────────────────── */

static void apply_bitflip(uint8_t* data, uint8_t len, uint32_t index) {
    if(len == 0) return;
    /* Flip bit at position (index % total_bits) */
    uint32_t total_bits = (uint32_t)len * 8;
    uint32_t bit_pos = index % total_bits;
    uint32_t byte_idx = bit_pos / 8;
    uint8_t bit_mask = (uint8_t)(1 << (bit_pos % 8));
    data[byte_idx] ^= bit_mask;
}

/* ═════════════════════════════════════════════════
 * UID PROFILE
 * Generates UIDs of 4, 7, and 10 byte lengths.
 * ═════════════════════════════════════════════════ */

/* Baseline UIDs */
static const uint8_t uid_baseline_4[] = {0x04, 0x01, 0x02, 0x03};
static const uint8_t uid_baseline_7[] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
/* uid_baseline_10 removed — ISO14443 UIDs are 4 or 7 bytes in practice */

/*
 * Sequential: 256 x 4-byte (vary last byte) + 256 x 7-byte + special UIDs
 * = 512 + boundary cases
 */
#define UID_SEQ_4_COUNT   256
#define UID_SEQ_7_COUNT   256
#define UID_SPECIAL_COUNT 6
#define UID_SEQ_TOTAL     (UID_SEQ_4_COUNT + UID_SEQ_7_COUNT + UID_SPECIAL_COUNT)

static bool uid_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= UID_SEQ_TOTAL) return false;

    if(index < UID_SEQ_4_COUNT) {
        /* 4-byte UID: vary last byte 0x00..0xFF */
        out->data_len = 4;
        memcpy(out->data, uid_baseline_4, 4);
        out->data[3] = (uint8_t)(index & 0xFF);
    } else if(index < UID_SEQ_4_COUNT + UID_SEQ_7_COUNT) {
        /* 7-byte UID: vary last byte */
        uint32_t sub = index - UID_SEQ_4_COUNT;
        out->data_len = 7;
        memcpy(out->data, uid_baseline_7, 7);
        out->data[6] = (uint8_t)(sub & 0xFF);
    } else {
        /* Special cases */
        uint32_t special = index - UID_SEQ_4_COUNT - UID_SEQ_7_COUNT;
        switch(special) {
        case 0: /* Zero UID 4-byte */
            out->data_len = 4;
            memset(out->data, 0x00, 4);
            break;
        case 1: /* Max UID 4-byte */
            out->data_len = 4;
            memset(out->data, 0xFF, 4);
            break;
        case 2: /* Zero UID 7-byte */
            out->data_len = 7;
            memset(out->data, 0x00, 7);
            break;
        case 3: /* Max UID 7-byte */
            out->data_len = 7;
            memset(out->data, 0xFF, 7);
            break;
        case 4: /* Zero UID 10-byte */
            out->data_len = 10;
            memset(out->data, 0x00, 10);
            break;
        case 5: /* Max UID 10-byte */
            out->data_len = 10;
            memset(out->data, 0xFF, 10);
            break;
        default:
            return false;
        }
    }
    return true;
}

static bool uid_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    /* Pick random length: 4, 7, or 10 */
    uint8_t lens[] = {4, 7, 10};
    uint8_t len = lens[prng_next() % 3];
    out->data_len = len;
    fill_random(out->data, len);
    return true;
}

static bool uid_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    /* Use 7-byte baseline, flip one bit at a time: 7*8 = 56 cases */
    uint32_t total_bits = 7 * 8;
    if(index >= total_bits) return false;
    out->data_len = 7;
    memcpy(out->data, uid_baseline_7, 7);
    apply_bitflip(out->data, 7, index);
    return true;
}

static bool uid_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    /* 4-byte and 7-byte UIDs filled with boundary values */
    /* 4-byte: BOUNDARY_BYTE_COUNT values, 7-byte: BOUNDARY_BYTE_COUNT values */
    uint32_t total = BOUNDARY_BYTE_COUNT * 2;
    if(index >= total) return false;

    if(index < BOUNDARY_BYTE_COUNT) {
        out->data_len = 4;
        memset(out->data, boundary_bytes[index], 4);
    } else {
        out->data_len = 7;
        memset(out->data, boundary_bytes[index - BOUNDARY_BYTE_COUNT], 7);
    }
    return true;
}

static bool uid_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return uid_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return uid_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return uid_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return uid_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t uid_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return UID_SEQ_TOTAL;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return 7 * 8;
    case NfcFuzzerStrategyBoundary:
        return BOUNDARY_BYTE_COUNT * 2;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * ATQA/SAK PROFILE
 * Tests invalid ATQA (2 bytes) + SAK (1 byte) combos
 * ═════════════════════════════════════════════════ */

/* Common valid ATQA values for reference */
static const uint8_t known_atqa[][2] = {
    {0x44, 0x00}, /* NTAG/Ultralight */
    {0x04, 0x00}, /* MIFARE Classic 1K */
    {0x02, 0x00}, /* MIFARE Classic 4K */
    {0x44, 0x03}, /* MIFARE DESFire */
    {0x04, 0x04}, /* MIFARE Plus */
};
#define KNOWN_ATQA_COUNT (sizeof(known_atqa) / sizeof(known_atqa[0]))

#define ATQA_SAK_SEQ_COUNT 512

static bool atqa_sak_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= ATQA_SAK_SEQ_COUNT) return false;
    /* Iterate ATQA byte0 through 0x00..0xFF with SAK=0, then SAK through 0x00..0xFF with ATQA=0x44,0x00 */
    out->data_len = 3;
    if(index < 256) {
        out->data[0] = (uint8_t)(index & 0xFF);
        out->data[1] = 0x00;
        out->data[2] = 0x00; /* SAK=0 */
    } else {
        out->data[0] = 0x44;
        out->data[1] = 0x00;
        out->data[2] = (uint8_t)((index - 256) & 0xFF);
    }
    return true;
}

static bool atqa_sak_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    out->data_len = 3;
    out->data[0] = prng_byte();
    out->data[1] = prng_byte();
    out->data[2] = prng_byte();
    return true;
}

static bool atqa_sak_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    /* Baseline: NTAG ATQA + SAK = 3 bytes, 24 bits */
    if(index >= 24) return false;
    out->data_len = 3;
    out->data[0] = 0x44;
    out->data[1] = 0x00;
    out->data[2] = 0x00;
    apply_bitflip(out->data, 3, index);
    return true;
}

static bool atqa_sak_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    /* Test boundary ATQA/SAK: all combos of boundary bytes in positions */
    /* 8 values for byte0 x 8 for SAK = 64 + known ATQA combos */
    uint32_t boundary_combos = BOUNDARY_BYTE_COUNT * BOUNDARY_BYTE_COUNT;
    uint32_t total = boundary_combos + KNOWN_ATQA_COUNT;
    if(index >= total) return false;

    out->data_len = 3;
    if(index < boundary_combos) {
        uint32_t atqa_idx = index / BOUNDARY_BYTE_COUNT;
        uint32_t sak_idx = index % BOUNDARY_BYTE_COUNT;
        out->data[0] = boundary_bytes[atqa_idx];
        out->data[1] = 0x00;
        out->data[2] = boundary_bytes[sak_idx];
    } else {
        uint32_t k = index - boundary_combos;
        out->data[0] = known_atqa[k][0];
        out->data[1] = known_atqa[k][1];
        out->data[2] = 0xFF; /* Invalid SAK with known ATQA */
    }
    return true;
}

static bool
    atqa_sak_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return atqa_sak_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return atqa_sak_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return atqa_sak_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return atqa_sak_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t atqa_sak_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return ATQA_SAK_SEQ_COUNT;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return 24;
    case NfcFuzzerStrategyBoundary:
        return (BOUNDARY_BYTE_COUNT * BOUNDARY_BYTE_COUNT) + KNOWN_ATQA_COUNT;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * FRAME PROFILE
 * Generates oversized, undersized, bad-CRC ISO14443-A frames
 * ═════════════════════════════════════════════════ */

/* Standard frame sizes for reference */
#define FRAME_MIN_LEN      1
#define FRAME_MAX_LEN      64
#define FRAME_BASELINE_LEN 16

/* A normal ACK byte */
static const uint8_t frame_baseline[FRAME_BASELINE_LEN] = {
    0xA0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xCE,
};

#define FRAME_SEQ_OVERSIZED  32
#define FRAME_SEQ_UNDERSIZED 4
#define FRAME_SEQ_BAD_CRC    16
#define FRAME_SEQ_TOTAL      (FRAME_SEQ_OVERSIZED + FRAME_SEQ_UNDERSIZED + FRAME_SEQ_BAD_CRC)

static bool frame_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= FRAME_SEQ_TOTAL) return false;

    if(index < FRAME_SEQ_OVERSIZED) {
        /* Oversized frames: baseline + padding of increasing length */
        uint8_t extra = (uint8_t)(index + 1);
        uint8_t total_len = FRAME_BASELINE_LEN + extra;
        memcpy(out->data, frame_baseline, FRAME_BASELINE_LEN);
        for(uint8_t i = FRAME_BASELINE_LEN; i < total_len; i++) {
            out->data[i] = (uint8_t)(0xAA + i);
        }
        out->data_len = total_len;
    } else if(index < FRAME_SEQ_OVERSIZED + FRAME_SEQ_UNDERSIZED) {
        /* Undersized frames: 1-byte, 2-byte, 3-byte, empty-ish */
        uint32_t sub = index - FRAME_SEQ_OVERSIZED;
        uint8_t len = (uint8_t)(sub + 1);
        memcpy(out->data, frame_baseline, len);
        out->data_len = len;
    } else {
        /* Bad CRC frames: valid frame with corrupted last 1-2 bytes */
        uint32_t sub = index - FRAME_SEQ_OVERSIZED - FRAME_SEQ_UNDERSIZED;
        memcpy(out->data, frame_baseline, FRAME_BASELINE_LEN);
        out->data_len = FRAME_BASELINE_LEN;
        /* Corrupt CRC byte(s) */
        out->data[FRAME_BASELINE_LEN - 1] ^= (uint8_t)(0x01 << (sub % 8));
        if(sub >= 8) {
            out->data[FRAME_BASELINE_LEN - 2] ^= (uint8_t)(0x01 << ((sub - 8) % 8));
        }
    }
    return true;
}

static bool frame_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    /* Random length 1..64, random content */
    uint8_t len = (uint8_t)((prng_next() % FRAME_MAX_LEN) + 1);
    out->data_len = len;
    fill_random(out->data, len);
    return true;
}

static bool frame_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    uint32_t total_bits = FRAME_BASELINE_LEN * 8;
    if(index >= total_bits) return false;
    memcpy(out->data, frame_baseline, FRAME_BASELINE_LEN);
    out->data_len = FRAME_BASELINE_LEN;
    apply_bitflip(out->data, FRAME_BASELINE_LEN, index);
    return true;
}

static bool frame_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    /* Boundary lengths: 0, 1, 2, max-1, max, max+1; filled with boundary bytes */
    static const uint8_t boundary_lens[] = {1, 2, 3, 63, 64, 65};
    uint32_t len_count = sizeof(boundary_lens) / sizeof(boundary_lens[0]);
    uint32_t total = len_count * BOUNDARY_BYTE_COUNT;
    if(index >= total) return false;

    uint32_t len_idx = index / BOUNDARY_BYTE_COUNT;
    uint32_t fill_idx = index % BOUNDARY_BYTE_COUNT;

    uint8_t len = boundary_lens[len_idx];
    memset(out->data, boundary_bytes[fill_idx], len);
    out->data_len = len;
    return true;
}

static bool
    frame_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return frame_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return frame_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return frame_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return frame_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t frame_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return FRAME_SEQ_TOTAL;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return FRAME_BASELINE_LEN * 8;
    case NfcFuzzerStrategyBoundary:
        return 6 * BOUNDARY_BYTE_COUNT;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * NTAG PROFILE
 * Out-of-bounds page reads, bad capability container
 * ═════════════════════════════════════════════════ */

/*
 * NTAG READ command: 0x30 <page_num>
 * NTAG WRITE command: 0xA2 <page_num> <4 data bytes>
 * Valid pages for NTAG213: 0-44, NTAG215: 0-134, NTAG216: 0-230
 */

#define NTAG_READ_CMD  0x30
#define NTAG_WRITE_CMD 0xA2
#define NTAG_SEQ_TOTAL 300

static bool ntag_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= NTAG_SEQ_TOTAL) return false;

    if(index < 256) {
        /* READ all page numbers 0x00 - 0xFF */
        out->data_len = 2;
        out->data[0] = NTAG_READ_CMD;
        out->data[1] = (uint8_t)(index & 0xFF);
    } else if(index < 256 + 32) {
        /* WRITE to OOB pages with data */
        uint32_t sub = index - 256;
        out->data_len = 6;
        out->data[0] = NTAG_WRITE_CMD;
        out->data[1] = (uint8_t)(0xE0 + sub); /* Pages 0xE0+ are invalid */
        out->data[2] = 0xDE;
        out->data[3] = 0xAD;
        out->data[4] = 0xBE;
        out->data[5] = 0xEF;
    } else {
        /* Bad CC (Capability Container) page 3 writes */
        uint32_t sub = index - 256 - 32;
        out->data_len = 6;
        out->data[0] = NTAG_WRITE_CMD;
        out->data[1] = 0x03; /* CC page */
        /* Vary CC bytes */
        out->data[2] = (uint8_t)(sub * 0x11);
        out->data[3] = (uint8_t)(sub * 0x22);
        out->data[4] = (uint8_t)(sub * 0x33);
        out->data[5] = (uint8_t)(sub * 0x44);
    }
    return true;
}

static bool ntag_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    if(prng_next() & 1) {
        /* Random READ */
        out->data_len = 2;
        out->data[0] = NTAG_READ_CMD;
        out->data[1] = prng_byte();
    } else {
        /* Random WRITE */
        out->data_len = 6;
        out->data[0] = NTAG_WRITE_CMD;
        out->data[1] = prng_byte();
        fill_random(out->data + 2, 4);
    }
    return true;
}

static bool ntag_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    /* Baseline: READ page 0 */
    uint8_t baseline[] = {NTAG_READ_CMD, 0x00};
    uint32_t total_bits = 2 * 8;
    if(index >= total_bits) return false;
    out->data_len = 2;
    memcpy(out->data, baseline, 2);
    apply_bitflip(out->data, 2, index);
    return true;
}

static bool ntag_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    /* Boundary page numbers with READ + WRITE */
    static const uint8_t boundary_pages[] = {
        0x00,
        0x01,
        0x02,
        0x03,
        0x04, /* Config/CC pages */
        0x2C,
        0x2D, /* NTAG213 boundary */
        0x86,
        0x87, /* NTAG215 boundary */
        0xE6,
        0xE7, /* NTAG216 boundary */
        0xFE,
        0xFF, /* Absolute max */
    };
    uint32_t page_count = sizeof(boundary_pages) / sizeof(boundary_pages[0]);
    uint32_t total = page_count * 2; /* READ + WRITE for each */
    if(index >= total) return false;

    uint32_t page_idx = index / 2;
    bool is_write = (index % 2) == 1;

    if(is_write) {
        out->data_len = 6;
        out->data[0] = NTAG_WRITE_CMD;
        out->data[1] = boundary_pages[page_idx];
        out->data[2] = 0xFF;
        out->data[3] = 0xFF;
        out->data[4] = 0xFF;
        out->data[5] = 0xFF;
    } else {
        out->data_len = 2;
        out->data[0] = NTAG_READ_CMD;
        out->data[1] = boundary_pages[page_idx];
    }
    return true;
}

static bool ntag_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return ntag_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return ntag_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return ntag_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return ntag_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t ntag_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return NTAG_SEQ_TOTAL;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return 16;
    case NfcFuzzerStrategyBoundary:
        return 13 * 2;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * ISO15693 PROFILE
 * Malformed inventory responses and system info
 * ═════════════════════════════════════════════════ */

/*
 * ISO15693 inventory response format:
 * Flags (1) + DSFID (1) + UID (8)
 */

#define ISO15693_INV_RESP_LEN 10
#define ISO15693_SEQ_TOTAL    280

static bool iso15693_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= ISO15693_SEQ_TOTAL) return false;

    if(index < 256) {
        /* Vary flags byte 0x00..0xFF with valid UID */
        out->data_len = ISO15693_INV_RESP_LEN;
        out->data[0] = (uint8_t)(index & 0xFF); /* Flags */
        out->data[1] = 0x00; /* DSFID */
        /* UID */
        out->data[2] = 0xE0;
        out->data[3] = 0x04;
        out->data[4] = 0x01;
        out->data[5] = 0x02;
        out->data[6] = 0x03;
        out->data[7] = 0x04;
        out->data[8] = 0x05;
        out->data[9] = 0x06;
    } else if(index < 256 + 8) {
        /* Truncated responses: 1..8 bytes */
        uint32_t sub = index - 256;
        uint8_t len = (uint8_t)(sub + 1);
        out->data_len = len;
        out->data[0] = 0x00; /* Flags */
        out->data[1] = 0x00; /* DSFID */
        for(uint8_t i = 2; i < len; i++) {
            out->data[i] = 0xAA;
        }
    } else {
        /* Oversized responses */
        uint32_t sub = index - 256 - 8;
        uint8_t len = (uint8_t)(ISO15693_INV_RESP_LEN + sub + 1);
        out->data_len = len;
        out->data[0] = 0x00;
        out->data[1] = 0x00;
        for(uint8_t i = 2; i < len; i++) {
            out->data[i] = (uint8_t)(0xBB + i);
        }
    }
    return true;
}

static bool iso15693_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    uint8_t len = (uint8_t)((prng_next() % 32) + 1);
    out->data_len = len;
    fill_random(out->data, len);
    return true;
}

static bool iso15693_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    uint32_t total_bits = ISO15693_INV_RESP_LEN * 8;
    if(index >= total_bits) return false;
    /* Baseline valid inventory response */
    out->data_len = ISO15693_INV_RESP_LEN;
    out->data[0] = 0x00;
    out->data[1] = 0x00;
    out->data[2] = 0xE0;
    out->data[3] = 0x04;
    out->data[4] = 0x01;
    out->data[5] = 0x02;
    out->data[6] = 0x03;
    out->data[7] = 0x04;
    out->data[8] = 0x05;
    out->data[9] = 0x06;
    apply_bitflip(out->data, ISO15693_INV_RESP_LEN, index);
    return true;
}

static bool iso15693_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    /* Fill the inventory response with boundary bytes */
    if(index >= BOUNDARY_BYTE_COUNT) return false;
    out->data_len = ISO15693_INV_RESP_LEN;
    memset(out->data, boundary_bytes[index], ISO15693_INV_RESP_LEN);
    return true;
}

static bool
    iso15693_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return iso15693_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return iso15693_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return iso15693_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return iso15693_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t iso15693_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return ISO15693_SEQ_TOTAL;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return ISO15693_INV_RESP_LEN * 8;
    case NfcFuzzerStrategyBoundary:
        return BOUNDARY_BYTE_COUNT;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * READER COMMANDS PROFILE (Poller mode)
 * Sends malformed SELECT, READ, WRITE commands to tags
 * ═════════════════════════════════════════════════ */

/* ISO14443-A commands */
#define CMD_REQA       0x26
#define CMD_WUPA       0x52
#define CMD_SELECT_CL1 0x93
#define CMD_SELECT_CL2 0x95
#define CMD_SELECT_CL3 0x97
#define CMD_RATS       0xE0
#define CMD_HLTA_1     0x50
#define CMD_HLTA_2     0x00

/* MIFARE commands */
#define CMD_MF_AUTH_A 0x60
#define CMD_MF_AUTH_B 0x61
#define CMD_MF_READ   0x30
#define CMD_MF_WRITE  0xA0

#define READER_CMD_SEQ_TOTAL 300

static bool reader_cmd_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= READER_CMD_SEQ_TOTAL) return false;

    if(index < 50) {
        /* Malformed SELECT CL1: vary NVB and UID bytes */
        out->data_len = 7;
        out->data[0] = CMD_SELECT_CL1;
        out->data[1] = (uint8_t)(0x20 + (index % 0x60)); /* NVB */
        fill_random(out->data + 2, 5);
    } else if(index < 100) {
        /* Malformed RATS: vary FSD/CID byte */
        uint32_t sub = index - 50;
        out->data_len = 2;
        out->data[0] = CMD_RATS;
        out->data[1] = (uint8_t)(sub * 5); /* Various FSD/CID combos */
    } else if(index < 200) {
        /* MIFARE READ with all block numbers 0x00..0x63 (100 blocks) */
        uint32_t sub = index - 100;
        out->data_len = 2;
        out->data[0] = CMD_MF_READ;
        out->data[1] = (uint8_t)(sub & 0xFF);
    } else if(index < 264) {
        /* MIFARE AUTH with all block numbers and key A */
        uint32_t sub = index - 200;
        out->data_len = 2;
        out->data[0] = CMD_MF_AUTH_A;
        out->data[1] = (uint8_t)(sub & 0xFF);
    } else {
        /* Completely random command bytes */
        uint32_t sub = index - 264;
        uint8_t len = (uint8_t)((sub % 8) + 1);
        out->data_len = len;
        fill_random(out->data, len);
    }
    return true;
}

static bool reader_cmd_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    uint8_t len = (uint8_t)((prng_next() % 16) + 1);
    out->data_len = len;
    fill_random(out->data, len);
    return true;
}

static bool reader_cmd_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    /* Baseline: SELECT CL1 with full UID */
    uint8_t baseline[] = {CMD_SELECT_CL1, 0x70, 0x04, 0x01, 0x02, 0x03, 0x04};
    uint32_t total_bits = sizeof(baseline) * 8;
    if(index >= total_bits) return false;
    out->data_len = sizeof(baseline);
    memcpy(out->data, baseline, sizeof(baseline));
    apply_bitflip(out->data, sizeof(baseline), index);
    return true;
}

static bool reader_cmd_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    /* Test boundary values for common commands */
    /* REQA/WUPA, SELECT with boundary bytes, RATS boundary, MF_READ boundary */
    static const uint8_t cmds[] = {
        CMD_REQA,
        CMD_WUPA,
        CMD_SELECT_CL1,
        CMD_SELECT_CL2,
        CMD_SELECT_CL3,
        CMD_RATS,
        CMD_MF_AUTH_A,
        CMD_MF_AUTH_B,
        CMD_MF_READ,
        CMD_MF_WRITE,
    };
    uint32_t cmd_count = sizeof(cmds) / sizeof(cmds[0]);
    uint32_t total = cmd_count * BOUNDARY_BYTE_COUNT;
    if(index >= total) return false;

    uint32_t cmd_idx = index / BOUNDARY_BYTE_COUNT;
    uint32_t param_idx = index % BOUNDARY_BYTE_COUNT;

    out->data_len = 2;
    out->data[0] = cmds[cmd_idx];
    out->data[1] = boundary_bytes[param_idx];
    return true;
}

static bool
    reader_cmd_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return reader_cmd_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return reader_cmd_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return reader_cmd_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return reader_cmd_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t reader_cmd_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return READER_CMD_SEQ_TOTAL;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return 7 * 8;
    case NfcFuzzerStrategyBoundary:
        return 10 * BOUNDARY_BYTE_COUNT;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * MIFARE AUTH PROFILE (Poller mode)
 * AUTH_A/AUTH_B commands with all block numbers and
 * well-known keys. All frames sent to tags.
 * ═════════════════════════════════════════════════ */

/* 8 well-known MIFARE Classic keys */
static const uint8_t known_mifare_keys[][6] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* Factory default */
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}, /* MAD key A */
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5}, /* MAD key B */
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}, /* NFC Forum */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* All zeros */
    {0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0}, /* Common */
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD}, /* NDEF */
    {0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A}, /* Transport */
};
#define KNOWN_MIFARE_KEY_COUNT (sizeof(known_mifare_keys) / sizeof(known_mifare_keys[0]))

/* Sequential: 256 AUTH_A + 256 AUTH_B, all blocks, default key */
#define MIFARE_AUTH_SEQ_TOTAL        512
/* Boundary: 4 variants per key (AUTH_A blk 0, AUTH_A blk 3, AUTH_B blk 0, AUTH_B blk 3) */
#define MIFARE_AUTH_BOUNDARY_PER_KEY 4
#define MIFARE_AUTH_BOUNDARY_TOTAL   (KNOWN_MIFARE_KEY_COUNT * MIFARE_AUTH_BOUNDARY_PER_KEY)

static const uint8_t mifare_auth_default_key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool mifare_auth_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= MIFARE_AUTH_SEQ_TOTAL) return false;

    /* Frames: cmd (1) + block (1) + key (6) = 8 bytes */
    out->data_len = 8;
    out->data[0] = (index < 256) ? CMD_MF_AUTH_A : CMD_MF_AUTH_B;
    out->data[1] = (uint8_t)(index & 0xFF); /* block 0x00..0xFF */
    memcpy(out->data + 2, mifare_auth_default_key, 6);
    return true;
}

static bool mifare_auth_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    out->data_len = 8;
    out->data[0] = (prng_next() & 1) ? CMD_MF_AUTH_A : CMD_MF_AUTH_B;
    out->data[1] = prng_byte(); /* random block */
    fill_random(out->data + 2, 6); /* random key */
    return true;
}

static bool mifare_auth_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    /* Baseline: AUTH_A, block 0x00, key FF FF FF FF FF FF (8 bytes = 64 bits) */
    uint32_t total_bits = 8 * 8;
    if(index >= total_bits) return false;
    out->data_len = 8;
    out->data[0] = CMD_MF_AUTH_A;
    out->data[1] = 0x00;
    memcpy(out->data + 2, mifare_auth_default_key, 6);
    apply_bitflip(out->data, 8, index);
    return true;
}

static bool mifare_auth_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    /*
     * 4 cases per known key:
     *   0: AUTH_A block 0
     *   1: AUTH_A sector trailer (block 3)
     *   2: AUTH_B block 0
     *   3: AUTH_B sector trailer (block 3)
     */
    if(index >= MIFARE_AUTH_BOUNDARY_TOTAL) return false;

    uint32_t key_idx = index / MIFARE_AUTH_BOUNDARY_PER_KEY;
    uint32_t variant = index % MIFARE_AUTH_BOUNDARY_PER_KEY;

    out->data_len = 8;
    out->data[0] = (variant < 2) ? CMD_MF_AUTH_A : CMD_MF_AUTH_B;
    out->data[1] = (variant & 1) ? 0x03 : 0x00; /* sector trailer or block 0 */
    memcpy(out->data + 2, known_mifare_keys[key_idx], 6);
    return true;
}

static bool
    mifare_auth_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return mifare_auth_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return mifare_auth_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return mifare_auth_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return mifare_auth_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t mifare_auth_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return MIFARE_AUTH_SEQ_TOTAL;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return 8 * 8;
    case NfcFuzzerStrategyBoundary:
        return MIFARE_AUTH_BOUNDARY_TOTAL;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * MIFARE READ/WRITE PROFILE (Poller mode)
 * MF_READ all blocks + MF_WRITE to first 64 blocks
 * with pattern data. Boundary hits sector trailers.
 * ═════════════════════════════════════════════════ */

/* WRITE payload: 0xDE 0xAD repeated over 16 bytes */
static const uint8_t mifare_write_pattern[16] = {
    0xDE,
    0xAD,
    0xDE,
    0xAD,
    0xDE,
    0xAD,
    0xDE,
    0xAD,
    0xDE,
    0xAD,
    0xDE,
    0xAD,
    0xDE,
    0xAD,
    0xDE,
    0xAD,
};

/*
 * Sequential layout:
 *   0-255:   READ blocks 0x00..0xFF (2 bytes each)
 *   256-319: WRITE blocks 0x00..0x3F with pattern (18 bytes each)
 */
#define MIFARE_READ_SEQ_READ_COUNT  256
#define MIFARE_READ_SEQ_WRITE_COUNT 64
#define MIFARE_READ_SEQ_TOTAL       (MIFARE_READ_SEQ_READ_COUNT + MIFARE_READ_SEQ_WRITE_COUNT)

/*
 * Boundary block numbers:
 *   - Hard boundaries: 0, 1, 3, 4, 63, 64, 127, 128, 255
 *   - Sector trailers: blocks 3, 7, 11, ..., 63 (every 4th block starting at 3, 16 total)
 *   Total: 9 hard + 16 trailers = 25 unique blocks, each READ+WRITE = 50 cases
 */
static const uint8_t mifare_rw_hard_boundary_blocks[] = {
    0x00,
    0x01,
    0x03,
    0x04,
    0x3F,
    0x40,
    0x7F,
    0x80,
    0xFF,
};
#define MIFARE_RW_HARD_BOUNDARY_COUNT \
    (sizeof(mifare_rw_hard_boundary_blocks) / sizeof(mifare_rw_hard_boundary_blocks[0]))

/* 16 sector trailers: block 3, 7, 11, ..., 63 */
#define MIFARE_RW_TRAILER_COUNT   16
#define MIFARE_RW_BOUNDARY_BLOCKS (MIFARE_RW_HARD_BOUNDARY_COUNT + MIFARE_RW_TRAILER_COUNT)
#define MIFARE_RW_BOUNDARY_TOTAL  (MIFARE_RW_BOUNDARY_BLOCKS * 2) /* READ + WRITE each */

static uint8_t mifare_rw_boundary_block(uint32_t idx) {
    if(idx < MIFARE_RW_HARD_BOUNDARY_COUNT) {
        return mifare_rw_hard_boundary_blocks[idx];
    }
    /* Sector trailer idx: trailer N is block (N*4 + 3), N = 0..15 */
    uint32_t trailer_idx = idx - MIFARE_RW_HARD_BOUNDARY_COUNT;
    return (uint8_t)(trailer_idx * 4 + 3);
}

static bool mifare_read_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= MIFARE_READ_SEQ_TOTAL) return false;

    if(index < MIFARE_READ_SEQ_READ_COUNT) {
        /* READ block: cmd (1) + block (1) = 2 bytes */
        out->data_len = 2;
        out->data[0] = CMD_MF_READ;
        out->data[1] = (uint8_t)(index & 0xFF);
    } else {
        /* WRITE block: cmd (1) + block (1) + data (16) = 18 bytes */
        uint32_t sub = index - MIFARE_READ_SEQ_READ_COUNT;
        out->data_len = 18;
        out->data[0] = CMD_MF_WRITE;
        out->data[1] = (uint8_t)(sub & 0x3F); /* blocks 0-63 */
        memcpy(out->data + 2, mifare_write_pattern, 16);
    }
    return true;
}

static bool mifare_read_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    if(prng_next() & 1) {
        /* Random READ */
        out->data_len = 2;
        out->data[0] = CMD_MF_READ;
        out->data[1] = prng_byte();
    } else {
        /* Random WRITE */
        out->data_len = 18;
        out->data[0] = CMD_MF_WRITE;
        out->data[1] = prng_byte();
        fill_random(out->data + 2, 16);
    }
    return true;
}

static bool mifare_read_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    /* Baseline: READ block 0 (2 bytes = 16 bits) */
    uint32_t total_bits = 2 * 8;
    if(index >= total_bits) return false;
    out->data_len = 2;
    out->data[0] = CMD_MF_READ;
    out->data[1] = 0x00;
    apply_bitflip(out->data, 2, index);
    return true;
}

static bool mifare_read_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= MIFARE_RW_BOUNDARY_TOTAL) return false;

    uint32_t block_idx = index / 2;
    bool is_write = (index % 2) == 1;
    uint8_t block = mifare_rw_boundary_block(block_idx);

    if(is_write) {
        out->data_len = 18;
        out->data[0] = CMD_MF_WRITE;
        out->data[1] = block;
        memcpy(out->data + 2, mifare_write_pattern, 16);
    } else {
        out->data_len = 2;
        out->data[0] = CMD_MF_READ;
        out->data[1] = block;
    }
    return true;
}

static bool
    mifare_read_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return mifare_read_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return mifare_read_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return mifare_read_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return mifare_read_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t mifare_read_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return MIFARE_READ_SEQ_TOTAL;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return 2 * 8;
    case NfcFuzzerStrategyBoundary:
        return MIFARE_RW_BOUNDARY_TOTAL;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * RATS/ATS PROFILE (Poller mode)
 * RATS parameter byte exhaustion + PPS fuzzing +
 * truncated/oversized frame variants.
 * ═════════════════════════════════════════════════ */

/*
 * RATS frame:  0xE0 + param  (2 bytes)
 *   param bits 7-4 = FSDI (frame size device -> tag)
 *   param bits 3-0 = CID  (card identifier, 0-14 valid, 15 reserved)
 *
 * PPS frame:   0xD0|CID + PPS0 + PPS1  (3 bytes)
 *   PPS0 = 0x11 selects PPS1 present + D and DR fields
 *   PPS1 = 0x00 => DS=DR=106 kbps (default)
 */

#define CMD_PPS_BASE 0xD0

/*
 * Sequential layout:
 *   0-255:   RATS with all param bytes 0x00..0xFF
 *   256-271: PPS for CID 0-15, PPS0=0x11, PPS1=0x00
 *   272-319: Truncated (1-byte) and oversized (3-16 byte) RATS frames (48 cases)
 */
#define RATS_SEQ_RATS_COUNT      256
#define RATS_SEQ_PPS_COUNT       16
#define RATS_SEQ_MALFORMED_COUNT 48
#define RATS_SEQ_TOTAL           (RATS_SEQ_RATS_COUNT + RATS_SEQ_PPS_COUNT + RATS_SEQ_MALFORMED_COUNT)

/*
 * Boundary layout:
 *   FSDI boundary values: 0, 7, 8, 15 (4 values)
 *   CID boundary values:  0, 14, 15   (3 values)
 *   12 RATS combos (4 x 3) + 6 boundary PPS (CID 0,14,15 x PPS0 0x00/0xFF) = 18 cases
 */
static const uint8_t rats_boundary_fsdi[] = {0, 7, 8, 15};
static const uint8_t rats_boundary_cid[] = {0, 14, 15};
#define RATS_BOUNDARY_FSDI_COUNT (sizeof(rats_boundary_fsdi) / sizeof(rats_boundary_fsdi[0]))
#define RATS_BOUNDARY_CID_COUNT  (sizeof(rats_boundary_cid) / sizeof(rats_boundary_cid[0]))
#define RATS_BOUNDARY_RATS_COUNT (RATS_BOUNDARY_FSDI_COUNT * RATS_BOUNDARY_CID_COUNT)
/* PPS boundary: 3 CID values x 2 PPS0 extremes (0x00, 0xFF) */
#define RATS_BOUNDARY_PPS_COUNT  (RATS_BOUNDARY_CID_COUNT * 2)
#define RATS_BOUNDARY_TOTAL      (RATS_BOUNDARY_RATS_COUNT + RATS_BOUNDARY_PPS_COUNT)

static bool rats_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= RATS_SEQ_TOTAL) return false;

    if(index < RATS_SEQ_RATS_COUNT) {
        /* RATS with all 256 parameter byte values */
        out->data_len = 2;
        out->data[0] = CMD_RATS;
        out->data[1] = (uint8_t)(index & 0xFF);
    } else if(index < RATS_SEQ_RATS_COUNT + RATS_SEQ_PPS_COUNT) {
        /* PPS for CID 0-15, fixed PPS0=0x11, PPS1=0x00 */
        uint32_t cid = index - RATS_SEQ_RATS_COUNT;
        out->data_len = 3;
        out->data[0] = (uint8_t)(CMD_PPS_BASE | (cid & 0x0F));
        out->data[1] = 0x11; /* PPS0: D and DR fields present */
        out->data[2] = 0x00; /* PPS1: DS=DR=106 kbps */
    } else {
        /* Truncated and oversized RATS frames */
        uint32_t sub = index - RATS_SEQ_RATS_COUNT - RATS_SEQ_PPS_COUNT;
        if(sub < 1) {
            /* 1-byte frame: just the command byte, no param */
            out->data_len = 1;
            out->data[0] = CMD_RATS;
        } else {
            /* Oversized: 3 to 50 bytes */
            uint32_t raw_len = sub + 2;
            if(raw_len > NFC_FUZZER_MAX_PAYLOAD_LEN) raw_len = NFC_FUZZER_MAX_PAYLOAD_LEN;
            uint8_t len = (uint8_t)raw_len;
            out->data_len = len;
            out->data[0] = CMD_RATS;
            out->data[1] = 0x50; /* valid-ish param */
            for(uint8_t i = 2; i < len; i++) {
                out->data[i] = (uint8_t)(0xAA + i); /* padding pattern */
            }
        }
    }
    return true;
}

static bool rats_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    if(prng_next() & 1) {
        /* Random RATS */
        out->data_len = 2;
        out->data[0] = CMD_RATS;
        out->data[1] = prng_byte();
    } else {
        /* Random PPS */
        out->data_len = 3;
        out->data[0] = (uint8_t)(CMD_PPS_BASE | (prng_byte() & 0x0F));
        out->data[1] = prng_byte();
        out->data[2] = prng_byte();
    }
    return true;
}

static bool rats_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    /* Baseline: RATS 0x50 — FSDI=5 (64-byte FSD), CID=0 (2 bytes = 16 bits) */
    uint32_t total_bits = 2 * 8;
    if(index >= total_bits) return false;
    out->data_len = 2;
    out->data[0] = CMD_RATS;
    out->data[1] = 0x50;
    apply_bitflip(out->data, 2, index);
    return true;
}

static bool rats_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= RATS_BOUNDARY_TOTAL) return false;

    if(index < RATS_BOUNDARY_RATS_COUNT) {
        /* 4 FSDI x 3 CID = 12 RATS boundary combos */
        uint32_t fsdi_idx = index / RATS_BOUNDARY_CID_COUNT;
        uint32_t cid_idx = index % RATS_BOUNDARY_CID_COUNT;
        out->data_len = 2;
        out->data[0] = CMD_RATS;
        out->data[1] = (uint8_t)(((rats_boundary_fsdi[fsdi_idx] & 0x0F) << 4) |
                                 (rats_boundary_cid[cid_idx] & 0x0F));
    } else {
        /* 3 CID x 2 PPS0 extremes = 8 PPS boundary cases */
        uint32_t sub = index - RATS_BOUNDARY_RATS_COUNT;
        uint32_t cid_idx = sub / 2;
        bool pps0_max = (sub % 2) == 1;
        out->data_len = 3;
        out->data[0] = (uint8_t)(CMD_PPS_BASE | (rats_boundary_cid[cid_idx] & 0x0F));
        out->data[1] = pps0_max ? 0xFF : 0x00;
        out->data[2] = pps0_max ? 0xFF : 0x00;
    }
    return true;
}

static bool rats_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return rats_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return rats_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return rats_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return rats_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t rats_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return RATS_SEQ_TOTAL;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return 2 * 8;
    case NfcFuzzerStrategyBoundary:
        return RATS_BOUNDARY_TOTAL;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * NFC-B PUPI PROFILE (Listener mode)
 * Fuzz the 4-byte PUPI in the ATQB response.
 * The worker restarts the ISO 14443-3b listener with
 * each new PUPI to test reader collision-resolution.
 * ═════════════════════════════════════════════════ */

#define NFCB_PUPI_LEN     4
#define NFCB_SEQ_TOTAL    256 /* vary low byte across 0x00..0xFF */
#define NFCB_BITFLIP_BITS (NFCB_PUPI_LEN * 8) /* 32 bits */

/* Baseline PUPI: 0x01 0x02 0x03 0x00 (last byte varied) */
static const uint8_t nfcb_pupi_baseline[NFCB_PUPI_LEN] = {0x01, 0x02, 0x03, 0x00};

static bool nfcb_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= NFCB_SEQ_TOTAL) return false;
    memcpy(out->data, nfcb_pupi_baseline, NFCB_PUPI_LEN);
    out->data[NFCB_PUPI_LEN - 1] = (uint8_t)(index & 0xFF);
    out->data_len = NFCB_PUPI_LEN;
    return true;
}

static bool nfcb_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    fill_random(out->data, NFCB_PUPI_LEN);
    out->data_len = NFCB_PUPI_LEN;
    return true;
}

static bool nfcb_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= NFCB_BITFLIP_BITS) return false;
    memcpy(out->data, nfcb_pupi_baseline, NFCB_PUPI_LEN);
    apply_bitflip(out->data, NFCB_PUPI_LEN, index);
    out->data_len = NFCB_PUPI_LEN;
    return true;
}

static bool nfcb_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= BOUNDARY_BYTE_COUNT) return false;
    memset(out->data, boundary_bytes[index], NFCB_PUPI_LEN);
    out->data_len = NFCB_PUPI_LEN;
    return true;
}

static bool nfcb_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return nfcb_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return nfcb_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return nfcb_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return nfcb_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t nfcb_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return NFCB_SEQ_TOTAL;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return NFCB_BITFLIP_BITS;
    case NfcFuzzerStrategyBoundary:
        return BOUNDARY_BYTE_COUNT;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * FELICA IDm PROFILE (Listener mode)
 * Fuzz the 8-byte IDm (Manufacture ID) in the FeliCa
 * polling response. Targets ISO 18092 / JIS X6319-4
 * readers such as transit gate controllers.
 *
 * IDm layout: [Manufacturer code 2B][Card ID 6B]
 * Common Sony manufacturer codes:
 *   0x0120 (Mobile Suica), 0x0130, 0x0428 (PASMO),
 *   0x012F (Lite-S), 0x88B4 (NFC-F test)
 * ═════════════════════════════════════════════════ */

#define FELICA_IDM_LEN      8
#define FELICA_BITFLIP_BITS (FELICA_IDM_LEN * 8) /* 64 bits */
#define FELICA_SEQ_TOTAL    256 /* vary low byte of card-ID section */

/* Common FeliCa manufacturer codes (big-endian) */
static const uint8_t felica_manufacturer_codes[][2] = {
    {0x01, 0x20}, /* Mobile Suica */
    {0x01, 0x30}, /* RC-S730/S728 */
    {0x04, 0x28}, /* PASMO */
    {0x01, 0x2F}, /* FeliCa Lite-S */
    {0x88, 0xB4}, /* NFC-F test card */
};
#define FELICA_MFR_CODE_COUNT \
    (sizeof(felica_manufacturer_codes) / sizeof(felica_manufacturer_codes[0]))

/* Baseline IDm: 0x0120 + sequential card-ID */
static const uint8_t felica_idm_baseline[FELICA_IDM_LEN] =
    {0x01, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static bool felica_profile_sequential(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= FELICA_SEQ_TOTAL) return false;
    memcpy(out->data, felica_idm_baseline, FELICA_IDM_LEN);
    out->data[FELICA_IDM_LEN - 1] = (uint8_t)(index & 0xFF);
    out->data_len = FELICA_IDM_LEN;
    return true;
}

static bool felica_profile_random(uint32_t index, NfcFuzzerTestCase* out) {
    (void)index;
    /* Pick a random (but realistic) manufacturer code then random card-ID */
    uint32_t mfr = prng_next() % FELICA_MFR_CODE_COUNT;
    out->data[0] = felica_manufacturer_codes[mfr][0];
    out->data[1] = felica_manufacturer_codes[mfr][1];
    fill_random(out->data + 2, FELICA_IDM_LEN - 2);
    out->data_len = FELICA_IDM_LEN;
    return true;
}

static bool felica_profile_bitflip(uint32_t index, NfcFuzzerTestCase* out) {
    if(index >= FELICA_BITFLIP_BITS) return false;
    memcpy(out->data, felica_idm_baseline, FELICA_IDM_LEN);
    apply_bitflip(out->data, FELICA_IDM_LEN, index);
    out->data_len = FELICA_IDM_LEN;
    return true;
}

static bool felica_profile_boundary(uint32_t index, NfcFuzzerTestCase* out) {
    /* Boundary: try all boundary bytes as manufacturer code + all-zero card ID,
     * then all boundary bytes as card ID (2 × BOUNDARY_BYTE_COUNT cases). */
    uint32_t total = 2 * BOUNDARY_BYTE_COUNT;
    if(index >= total) return false;
    memset(out->data, 0, FELICA_IDM_LEN);
    if(index < BOUNDARY_BYTE_COUNT) {
        /* Vary the manufacturer code first byte */
        out->data[0] = boundary_bytes[index];
        out->data[1] = boundary_bytes[index];
    } else {
        /* Vary all card-ID bytes with one boundary value */
        uint32_t sub = index - BOUNDARY_BYTE_COUNT;
        out->data[0] = felica_idm_baseline[0];
        out->data[1] = felica_idm_baseline[1];
        memset(out->data + 2, boundary_bytes[sub], FELICA_IDM_LEN - 2);
    }
    out->data_len = FELICA_IDM_LEN;
    return true;
}

static bool
    felica_profile_next(NfcFuzzerStrategy strategy, uint32_t index, NfcFuzzerTestCase* out) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return felica_profile_sequential(index, out);
    case NfcFuzzerStrategyRandom:
        return felica_profile_random(index, out);
    case NfcFuzzerStrategyBitflip:
        return felica_profile_bitflip(index, out);
    case NfcFuzzerStrategyBoundary:
        return felica_profile_boundary(index, out);
    default:
        return false;
    }
}

static uint32_t felica_profile_total(NfcFuzzerStrategy strategy) {
    switch(strategy) {
    case NfcFuzzerStrategySequential:
        return FELICA_SEQ_TOTAL;
    case NfcFuzzerStrategyRandom:
        return UINT32_MAX;
    case NfcFuzzerStrategyBitflip:
        return FELICA_BITFLIP_BITS;
    case NfcFuzzerStrategyBoundary:
        return 2 * BOUNDARY_BYTE_COUNT;
    default:
        return 0;
    }
}

/* ═════════════════════════════════════════════════
 * Public dispatch API
 * ═════════════════════════════════════════════════ */

void nfc_fuzzer_profile_init(NfcFuzzerProfile profile, NfcFuzzerStrategy strategy) {
    (void)profile;
    (void)strategy;
    /* Re-seed PRNG for reproducibility */
    prng_seed(0x4E464346); /* "NFCF" */
    FURI_LOG_I(PROFILES_TAG, "Profile init: profile=%d strategy=%d", profile, strategy);
}

bool nfc_fuzzer_profile_next(
    NfcFuzzerProfile profile,
    NfcFuzzerStrategy strategy,
    uint32_t index,
    NfcFuzzerTestCase* out) {
    furi_assert(out);
    memset(out, 0, sizeof(NfcFuzzerTestCase));

    switch(profile) {
    case NfcFuzzerProfileUid:
        return uid_profile_next(strategy, index, out);
    case NfcFuzzerProfileAtqaSak:
        return atqa_sak_profile_next(strategy, index, out);
    case NfcFuzzerProfileFrame:
        return frame_profile_next(strategy, index, out);
    case NfcFuzzerProfileNtag:
        return ntag_profile_next(strategy, index, out);
    case NfcFuzzerProfileIso15693:
        return iso15693_profile_next(strategy, index, out);
    case NfcFuzzerProfileReaderCommands:
        return reader_cmd_profile_next(strategy, index, out);
    case NfcFuzzerProfileMifareAuth:
        return mifare_auth_profile_next(strategy, index, out);
    case NfcFuzzerProfileMifareRead:
        return mifare_read_profile_next(strategy, index, out);
    case NfcFuzzerProfileRats:
        return rats_profile_next(strategy, index, out);
    case NfcFuzzerProfileNfcB:
        return nfcb_profile_next(strategy, index, out);
    case NfcFuzzerProfileFelica:
        return felica_profile_next(strategy, index, out);
    default:
        FURI_LOG_E(PROFILES_TAG, "Unknown profile: %d", profile);
        return false;
    }
}

uint32_t nfc_fuzzer_profile_total_cases(NfcFuzzerProfile profile, NfcFuzzerStrategy strategy) {
    switch(profile) {
    case NfcFuzzerProfileUid:
        return uid_profile_total(strategy);
    case NfcFuzzerProfileAtqaSak:
        return atqa_sak_profile_total(strategy);
    case NfcFuzzerProfileFrame:
        return frame_profile_total(strategy);
    case NfcFuzzerProfileNtag:
        return ntag_profile_total(strategy);
    case NfcFuzzerProfileIso15693:
        return iso15693_profile_total(strategy);
    case NfcFuzzerProfileReaderCommands:
        return reader_cmd_profile_total(strategy);
    case NfcFuzzerProfileMifareAuth:
        return mifare_auth_profile_total(strategy);
    case NfcFuzzerProfileMifareRead:
        return mifare_read_profile_total(strategy);
    case NfcFuzzerProfileRats:
        return rats_profile_total(strategy);
    case NfcFuzzerProfileNfcB:
        return nfcb_profile_total(strategy);
    case NfcFuzzerProfileFelica:
        return felica_profile_total(strategy);
    default:
        return 0;
    }
}
