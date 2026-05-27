/* GWF water (Schweizerische Gas- und Wasserwerke / GWF MessSysteme,
 * Switzerland). Port of `_refs/wmbusmeters/src/driver_gwfwater.cc`.
 *
 * The frame is OMS-conformant up to a DIF=0x0F marker; the manufacturer
 * trailer carries a small status block:
 *
 *     bytes[0]   = type byte (must be 0x01 for the documented format)
 *     bytes[1]   = alarm bitmap
 *                  bit 1 = continuous flow
 *                  bit 3 = broken pipe
 *                  bit 5 = battery low
 *                  bit 6 = backflow
 *     bytes[2]   = power-mode bit + battery half-years
 *                  bit 0   = power-saving mode (else NORMAL)
 *                  bits 3+ = battery_remaining (semesters → /2 = years)
 *
 * Total volume + target volume + target date all come out of the OMS
 * walker (DIF 04 13, 44 13, 42 6C). We just append battery / power /
 * alarm rows after walking. */

#include "../../engine/driver.h"
#include "../../_oms_split.h"

#include <stdio.h>
#include <stdint.h>

static size_t decode_gwf(const WmbusDecodeCtx* ctx, char* out, size_t cap) {
    const uint8_t* m = NULL; size_t mlen = 0; size_t off = 0;
    size_t pos = oms_split_emit("GWF water", ctx->ci,
                                ctx->apdu, ctx->apdu_len,
                                out, cap,
                                &m, &mlen, &off);
    if(!m || mlen < 3) return pos;

    uint8_t type = m[0], a = m[1], b = m[2];
    if(type == 0x01) {
        char alarms[20] = {0}; size_t ap = 0;
#define APP(flag, txt) do { if((flag) && ap + sizeof(txt) < sizeof(alarms)) { \
            if(ap) alarms[ap++] = ','; \
            for(const char* s = txt; *s; s++) alarms[ap++] = *s; \
            alarms[ap] = 0; \
        } } while(0)
        APP(a & 0x02, "flow");
        APP(a & 0x08, "pipe");
        APP(a & 0x20, "batt");
        APP(a & 0x40, "back");
#undef APP
        pos += (size_t)snprintf(out + pos, cap - pos,
                                "Alarms %s\n"
                                "Mode  %s\n"
                                "Batt  %.1f y\n",
                                ap ? alarms : "OK",
                                (b & 0x01) ? "saving" : "normal",
                                (double)(b >> 3) / (double)2.0);
    } else {
        pos += (size_t)snprintf(out + pos, cap - pos,
                                "MfctTyp %02X\n", (unsigned)type);
    }
    return pos;
}

static const WmbusMVT k_mvt[] = {
    {"GWF", 0x0E, 0x01},
    {"GWF", 0x07, 0x3C},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_gwf_water = {
    .id = "gwfwater", .title = "GWF water",
    .mvt = k_mvt, .decode_ex = decode_gwf,
};
