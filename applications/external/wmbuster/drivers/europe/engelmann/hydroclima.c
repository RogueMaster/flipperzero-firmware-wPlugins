/* Engelmann Hydroclima HCA — port of
 * `_refs/wmbusmeters/src/driver_hydroclima.cc`.
 *
 * Engelmann ships two firmware variants identifiable by which OMS
 * field appears in the *prefix*:
 *
 *   • RF-RKN0 — has a `036E` (current HCA) DIF/VIF entry. The
 *     manufacturer trailer then carries: frame identifier, status,
 *     time, date, average ambient T, max ambient T, max date, count,
 *     last-month avg ambient T, last-month avg heater T, indication-u,
 *     total indication-u.
 *
 *   • RF-RKN9 — does *not* expose `036E`. Trailer is twelve months of
 *     "last X month uc" plus two date fields.
 *
 * Temperatures pack into 16-bit centi-°C; we render °C with two
 * decimals. The trailer is bigger than our 16-row detail buffer, so we
 * surface the most useful fields and rely on the OK→raw toggle for
 * the rest. */

#include "../../engine/driver.h"
#include "../../_oms_split.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static double centi_temp(uint8_t lo, uint8_t hi) {
    int16_t v = (int16_t)(((uint16_t)hi << 8) | lo);
    return (double)v / (double)100.0;
}

static size_t append_pair_temp(char* o, size_t cap, size_t pos,
                               const char* lbl, uint8_t lo, uint8_t hi) {
    return pos + (size_t)snprintf(o + pos, cap - pos,
                                  "%s  %.2f C\n", lbl, centi_temp(lo, hi));
}

/* Heuristic: scan the OMS-prefix bytes for the marker `03 6E ..` which
 * indicates the RKN0 layout. Done outside the wmbusmeters dv_entries
 * map because we don't carry one — but the byte pattern is unambiguous
 * for the small DIF/VIF set Engelmann ever uses. */
static bool has_036e(const uint8_t* a, size_t cut) {
    for(size_t i = 0; i + 1 < cut; i++) {
        if(a[i] == 0x03 && a[i + 1] == 0x6E) return true;
    }
    return false;
}

static size_t decode_rkn0(char* o, size_t cap, size_t pos,
                          const uint8_t* m, size_t l) {
    if(l < 1) return pos;
    pos += (size_t)snprintf(o + pos, cap - pos,
                            "FrId  %02X\n", (unsigned)m[0]);
    if(l < 9) return pos;
    /* Skip status (2), time (2), date (2) — surface only what fits. */
    pos = append_pair_temp(o, cap, pos, "AvgAmb", m[7], m[8]);
    if(l < 11) return pos;
    pos = append_pair_temp(o, cap, pos, "MaxAmb", m[9], m[10]);
    if(l < 17) return pos;
    /* Skip max-date (2), num-measurements (2). */
    pos = append_pair_temp(o, cap, pos, "AvgAmbL", m[15], m[16]);
    if(l < 19) return pos;
    pos = append_pair_temp(o, cap, pos, "AvgHtL",  m[17], m[18]);
    return pos;
}

static size_t decode_rkn9(char* o, size_t cap, size_t pos,
                          const uint8_t* m, size_t l) {
    /* RKN9 starts with twelve consecutive 16-bit "last X month uc"
     * readings — surface the four most recent ones to fit on screen. */
    for(int month = 1; month <= 4 && (size_t)(month * 2 + 1) <= l; month++) {
        uint16_t v = (uint16_t)m[(month - 1) * 2] |
                     ((uint16_t)m[(month - 1) * 2 + 1] << 8);
        pos += (size_t)snprintf(o + pos, cap - pos,
                                "M-%d  %u u\n", month, (unsigned)v);
    }
    return pos;
}

static size_t decode_hydroclima(const WmbusDecodeCtx* ctx, char* o, size_t cap) {
    const uint8_t* m = NULL; size_t mlen = 0; size_t off = 0;
    size_t pos = oms_split_emit("Engelmann Hydroclima", ctx->ci,
                                ctx->apdu, ctx->apdu_len, o, cap,
                                &m, &mlen, &off);
    if(!m || mlen < 1) return pos;

    if(has_036e(ctx->apdu, off))
        return decode_rkn0(o, cap, pos, m, mlen);
    else
        return decode_rkn9(o, cap, pos, m, mlen);
}

static const WmbusMVT k_mvt[] = {
    /* MVT-list lives in `hydroclimav2.xmq` — wmbusmeters defers detection
     * to that XMQ companion driver. The triples below match what the XMQ
     * declares so we get the same coverage. */
    {"DEV", 0x33, 0x08},
    {"DEV", 0x35, 0x08},
    {"DME", 0x70, 0x08},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_engelmann_hydroclima = {
    .id = "hydroclima", .title = "Engelmann Hydroclima",
    .mvt = k_mvt, .decode_ex = decode_hydroclima,
};
