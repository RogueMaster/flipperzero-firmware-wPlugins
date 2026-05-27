/* Zenner EDC B.One (medium 0x0B) — port of
 * `_refs/wmbusmeters/src/driver_zenner0b.cc`.
 *
 * The frame is empty on the OMS side (no DIF/VIF) — everything lives in
 * the manufacturer trailer that follows the DIF=0x0F marker:
 *
 *     bytes[0..3]   status   (LE u32; 0 = OK)
 *     bytes[4..7]   target volume   (LE u32, units of 1/256000 m³)
 *     bytes[8..11]  total volume    (LE u32, units of 1/256000 m³)
 *
 * Total / target are scaled by 1/256000 so a count of 17.062 m³ is
 * stored as 4368 * 1024 ≈ 4 369 408 raw, which divided by 256000 gives
 * 17.0680… The original wmbusmeters constant is 1/256000 m³. */

#include "../../engine/driver.h"
#include "../../_oms_split.h"

#include <stdio.h>
#include <stdint.h>

static uint32_t le32(const uint8_t* p) {
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[1] << 8)  | (uint32_t)p[0];
}

static size_t decode_zenner0b(const WmbusDecodeCtx* ctx, char* out, size_t cap) {
    const uint8_t* m = NULL; size_t mlen = 0; size_t off = 0;
    size_t pos = oms_split_emit("Zenner B.One water", ctx->ci,
                                ctx->apdu, ctx->apdu_len,
                                out, cap, &m, &mlen, &off);
    if(!m || mlen < 12) return pos;

    uint32_t status = le32(m);
    uint32_t target = le32(m + 4);
    uint32_t total  = le32(m + 8);

    /* 1/256000 m³ resolution → millilitre granularity in three places. */
    double total_m3  = (double)total  / (double)256000.0;
    double target_m3 = (double)target / (double)256000.0;

    pos += (size_t)snprintf(out + pos, cap - pos,
                            "Total  %.3f m3\n"
                            "Target  %.3f m3\n"
                            "Status %s\n",
                            total_m3, target_m3,
                            status == 0 ? "OK" : "fault");
    if(status != 0) {
        pos += (size_t)snprintf(out + pos, cap - pos,
                                "StatHex %08lX\n", (unsigned long)status);
    }
    return pos;
}

static const WmbusMVT k_mvt[] = {
    {"ZRI", 0x16, 0x0B},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_zenner0b = {
    .id = "zenner0b", .title = "Zenner B.One water",
    .mvt = k_mvt, .decode_ex = decode_zenner0b,
};
