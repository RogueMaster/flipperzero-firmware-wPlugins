/* Techem MK-Radio 3a — port of `_refs/wmbusmeters/src/driver_mkradio3a.cc`.
 *
 * MK-Radio 3a is *not* OMS-conformant; it carries a wholly proprietary
 * payload behind the standard wM-Bus link header (CI byte 0xA2). The
 * full upstream driver decodes 12 historic months packed across the
 * 42-byte content, which is more than the Flipper screen can usefully
 * display — we surface the three most useful values:
 *
 *     content[2..3]   target_date (16-bit packed — day:5, month:4, year:8)
 *     content[4..6]   total_m3 / 10  (LE 24-bit)
 *     content[8..9]   current-month consumption depending on day-of-month
 *
 * The full historic series remains accessible via the OK→raw toggle. */

#include "../../engine/driver.h"

#include <stdio.h>
#include <stdint.h>

static size_t decode_mk3a(uint16_t m, uint8_t v, uint8_t med,
                          const uint8_t* a, size_t l,
                          char* o, size_t cap) {
    (void)m; (void)v; (void)med;
    size_t pos = (size_t)snprintf(o, cap, "Techem MK-Radio 3a\n");
    if(l < 7) {
        pos += (size_t)snprintf(o + pos, cap - pos,
                                "Bytes  %u\n"
                                "Status  short frame\n", (unsigned)l);
        return pos;
    }

    /* The "content" buffer wmbusmeters parses begins at apdu[0] for our
     * purposes — the upstream `extractPayload` strips off the same CI/TPL
     * header we already consume. */
    uint16_t curr_date = ((uint16_t)a[3] << 8) | a[2];
    unsigned d = (curr_date >> 0) & 0x1F;
    unsigned mo = (curr_date >> 5) & 0x0F;
    unsigned y = (curr_date >> 9) & 0xFF;

    uint32_t total_raw =
        ((uint32_t)a[6] << 16) | ((uint32_t)a[5] << 8) | (uint32_t)a[4];
    double total_m3 = (double)total_raw / (double)10.0;

    pos += (size_t)snprintf(o + pos, cap - pos,
                            "Total  %.1f m3\n"
                            "Target  20%02u-%02u-%02u\n",
                            total_m3, y, mo % 13, d % 32);

    /* Day-of-month gate matches wmbusmeters: byte 8 alone before mid-month,
     * (byte 8 + byte 9) afterwards. */
    if(l >= 10) {
        double curr_month;
        if(d <= 15) curr_month = (double)a[8] / (double)10.0;
        else        curr_month = (double)((unsigned)a[9] + (unsigned)a[8]) / (double)10.0;
        pos += (size_t)snprintf(o + pos, cap - pos,
                                "ThisMon  %.1f m3\n", curr_month);
    }
    return pos;
}

static const WmbusMVT k_mvt[] = {
    {"TCH", 0x72, 0x50},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_techem_mkradio3a = {
    .id = "mkradio3a", .title = "Techem MK-Radio 3a",
    .mvt = k_mvt, .decode = decode_mk3a,
};
