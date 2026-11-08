/* VW AG / Audi TPMS (Huf "Gen5" family, e.g. sensor 5Q0907275F).
 * See the LICENSE file for information about the license.
 *
 * 433.92 MHz 2-FSK, 25us chips (40 kbaud), Manchester coded (20 kbit/s).
 *
 * Preamble: at least 3 bytes of 0xAA chips, then the sync byte 0x59 chips
 * (01011001). Data follows the sync, Manchester "10" = 1, "01" = 0.
 *
 * Normal frame, 11 bytes:
 *   b0      brand (0x88 Huf newer, 0x03 Huf Gen5, 0x80 Continental, ...)
 *   b1..b4  sensor ID, big endian
 *   b5      pressure, kPa = raw * 2.45
 *   b6      temperature, C = raw - 52
 *   b7..b9  flags / sequence (unknown meaning)
 *   b10     CRC8 poly 0x2F init 0xAA over b0..b9
 *
 * Alert (short) frame, 8 bytes:
 *   b0 = 0x00, b1..b4 ID, b5 pressure, b6 temperature,
 *   b7 CRC8 poly 0x2F init 0xAA over b0..b6
 *
 * Since the receiver FSK polarity may be inverted with respect to the
 * above, we also search the inverted preamble+sync, and in that case
 * use the swapped Manchester patterns. The CRC decides. */

#include "../../app.h"

#define VW_CRC_POLY      0x2F
#define VW_CRC_INIT      0xAA
#define VW_LONG_LEN      11
#define VW_SHORT_LEN     8
#define VW_SYNC_CHIPS    24 /* 16 preamble chips + 8 sync chips. */
#define VW_MAX_SYNC_HITS 8 /* Max preamble+sync matches tried per polarity. */

typedef struct {
    const char* sync; /* Last two preamble bytes + sync byte, as chips. */
    const char* zero; /* Manchester pattern for a 0 bit. */
    const char* one; /* Manchester pattern for a 1 bit. */
} VWPolarity;

static const VWPolarity vw_polarities[2] = {
    {"101010101010101001011001", "01", "10"}, /* Normal. */
    {"010101010101010110100110", "10", "01"}, /* Inverted. */
};

/* Check if the 'decoded' bits in 'raw' form a valid frame. Return the
 * frame length in bytes (VW_LONG_LEN or VW_SHORT_LEN), or 0 if invalid. */
static uint32_t vw_validate_frame(const uint8_t* raw, uint32_t decoded) {
    if(decoded >= VW_LONG_LEN * 8 &&
       crc8(raw, VW_LONG_LEN - 1, VW_CRC_INIT, VW_CRC_POLY) == raw[VW_LONG_LEN - 1])
        return VW_LONG_LEN;
    if(decoded >= VW_SHORT_LEN * 8 && raw[0] == 0x00 &&
       crc8(raw, VW_SHORT_LEN - 1, VW_CRC_INIT, VW_CRC_POLY) == raw[VW_SHORT_LEN - 1])
        return VW_SHORT_LEN;
    return 0;
}

/* Search for a valid frame with the given polarity. On success fill
 * 'raw', set *start (preamble+sync offset) and *data_off (first data chip)
 * and return the frame length. Otherwise return 0. */
static uint32_t vw_find_frame(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    const VWPolarity* pol,
    uint8_t* raw,
    uint32_t* start,
    uint32_t* data_off) {
    uint32_t off = 0;
    for(int hits = 0; hits < VW_MAX_SYNC_HITS && off < numbits; hits++) {
        off = bitmap_seek_bits(bits, numbytes, off, numbits - off, pol->sync);
        if(off == BITMAP_SEEK_NOT_FOUND) return 0;

        uint32_t doff = off + VW_SYNC_CHIPS;
        memset(raw, 0, VW_LONG_LEN);
        uint32_t decoded =
            convert_from_line_code(raw, VW_LONG_LEN, bits, numbytes, doff, pol->zero, pol->one);
        uint32_t len = vw_validate_frame(raw, decoded);
        if(len) {
            *start = off;
            *data_off = doff;
            return len;
        }
        off++; /* Try the next match. */
    }
    return 0;
}

static bool decode(uint8_t* bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo* info) {
    /* Preamble+sync plus at least the short frame, Manchester coded. */
    if(numbits < VW_SYNC_CHIPS + VW_SHORT_LEN * 8 * 2) return false;

    uint8_t raw[VW_LONG_LEN];
    uint32_t start = 0, data_off = 0, len = 0;
    for(uint32_t p = 0; p < 2 && len == 0; p++) {
        len = vw_find_frame(bits, numbytes, numbits, &vw_polarities[p], raw, &start, &data_off);
    }
    if(len == 0) return false;
    FURI_LOG_E(TAG, "VW TPMS frame found, %lu bytes", (unsigned long)len);

    info->start_off = start;
    info->pulses_count = (data_off + len * 8 * 2) - start;

    float kpa = (float)raw[5] * 2.45f;
    int temp = (int)raw[6] - 52;

    fieldset_add_bytes(info->fieldset, "Tire ID", raw + 1, 4 * 2);
    fieldset_add_float(info->fieldset, "Pressure kpa", kpa, 2);
    fieldset_add_int(info->fieldset, "Temperature C", temp, 8);
    fieldset_add_hex(info->fieldset, "Brand", raw[0], 8);
    if(len == VW_LONG_LEN) {
        fieldset_add_bytes(info->fieldset, "Flags", raw + 7, 3 * 2);
    } else {
        fieldset_add_str(info->fieldset, "Alert", "yes", 3);
    }
    return true;
}

ProtoViewDecoder VWTPMSDecoder =
    {.name = "VW TPMS", .decode = decode, .get_fields = NULL, .build_message = NULL};
