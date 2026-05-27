/* BMeters Hydrodigit (Italian water meter, very widely deployed in
 * southern EU). Port of `_refs/wmbusmeters/src/driver_hydrodigit.cc`.
 *
 * The frame is OMS-conformant up to a DIF=0x0F manufacturer marker.
 * The walker handles the standard prefix (total volume, meter datetime,
 * etc.); we add the manufacturer trailer:
 *
 *     bytes[0]      frame_identifier — bitmap of present sections
 *                     bit 0 = battery voltage
 *                     bit 1 = fraud date  (3 BCD bytes YY MM DD)
 *                     bit 2 = backflow    (4 LE bytes, /1000 = m³)
 *                     bit 4 = monthly history (12 × 3 LE bytes /100 = m³)
 *                     bit 7 = water-loss / leak date (3 BCD)
 *
 * The 12 historic months would overflow our 16-row detail buffer, so we
 * only surface the 4 most recent months as Mn1..Mn4 — the full series
 * stays available via the OK→raw toggle. */

#include "../../engine/driver.h"
#include "../../_oms_split.h"

#include <stdio.h>
#include <stdint.h>

#define MASK_BATTERY   (1u << 0)
#define MASK_FRAUD     (1u << 1)
#define MASK_BACKFLOW  (1u << 2)
#define MASK_HISTORY   (1u << 4)
#define MASK_LEAK      (1u << 7)

/* wmbusmeters' fixed voltage lookup table, indexed by low nibble. */
static double get_voltage(uint8_t b) {
    switch(b & 0x0F) {
        case 0x01: return (double)1.90;
        case 0x02: return (double)2.10;
        case 0x03: return (double)2.20;
        case 0x04: return (double)2.30;
        case 0x05: return (double)2.40;
        case 0x06: return (double)2.50;
        case 0x07: return (double)2.65;
        case 0x08: return (double)2.80;
        case 0x09: return (double)2.90;
        case 0x0A: return (double)3.05;
        case 0x0B: return (double)3.20;
        case 0x0C: return (double)3.35;
        case 0x0D: return (double)3.50;
        default:   return (double)3.70;  /* 0x0, 0x0E, 0x0F */
    }
}

static size_t decode_hydrodigit(const WmbusDecodeCtx* ctx, char* o, size_t cap) {
    const uint8_t* m = NULL; size_t mlen = 0; size_t off = 0;
    size_t pos = oms_split_emit("BMeters Hydrodigit", ctx->ci,
                                ctx->apdu, ctx->apdu_len, o, cap,
                                &m, &mlen, &off);
    if(!m || mlen < 1) return pos;

    uint8_t fid = m[0];
    if(fid == 0x00) return pos;     /* nothing further announced */
    size_t i = 1;

    if((fid & MASK_BATTERY) && i < mlen) {
        pos += (size_t)snprintf(o + pos, cap - pos,
                                "Volt  %.2f V\n", get_voltage(m[i]));
        i++;
    }
    if((fid & MASK_FRAUD) && i + 2 < mlen) {
        /* BCD year/month/day. Year is 20YY for sane future-proofing. */
        pos += (size_t)snprintf(o + pos, cap - pos,
                                "Fraud  20%02X-%02X-%02X\n",
                                m[i], m[i + 1], m[i + 2]);
        i += 3;
    }
    if((fid & MASK_LEAK) && i + 2 < mlen) {
        pos += (size_t)snprintf(o + pos, cap - pos,
                                "Leak  20%02X-%02X-%02X\n",
                                m[i], m[i + 1], m[i + 2]);
        i += 3;
    }
    if((fid & MASK_BACKFLOW) && i + 3 < mlen) {
        uint32_t v = (uint32_t)m[i] | ((uint32_t)m[i + 1] << 8) |
                     ((uint32_t)m[i + 2] << 16) | ((uint32_t)m[i + 3] << 24);
        pos += (size_t)snprintf(o + pos, cap - pos,
                                "Backflow  %.3f m3\n",
                                (double)v / (double)1000.0);
        i += 4;
    }
    if((fid & MASK_HISTORY) && i + 35 < mlen) {
        for(int month = 1; month <= 4; month++) {
            uint32_t v = (uint32_t)m[i] | ((uint32_t)m[i + 1] << 8) |
                         ((uint32_t)m[i + 2] << 16);
            double m3 = (double)v / (double)100.0;
            if(m3 >= (double)100000.0) m3 = (double)0.0;  /* sentinel for pre-install */
            pos += (size_t)snprintf(o + pos, cap - pos,
                                    "Mn%d  %.2f m3\n", month, m3);
            i += 3;
        }
    }
    return pos;
}

static const WmbusMVT k_mvt[] = {
    /* All hydrodigit telegrams identify as BMeters water (manuf BMT). */
    {"BMT", 0x13, 0x07},
    {"BMT", 0x13, 0x06},     /* warm-water variant                       */
    {"BMT", 0x13, 0x16},     /* cold-water variant seen in test telegram */
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_bmeters_hydrodigit = {
    .id = "hydrodigit", .title = "BMeters Hydrodigit",
    .mvt = k_mvt, .decode_ex = decode_hydrodigit,
};
