/* Sontex RFM-TX1 water (carries the wM-Bus manufacturer code BMT —
 * "Bopp & Reuther" / "Bmeters"-derived rebadge). Port of
 * `_refs/wmbusmeters/src/driver_rfmtx1.cc`.
 *
 * Two firmware revisions exist:
 *   • New stock — totally OMS-conformant; the standard walker decodes
 *     `total_m3` and `meter_datetime` straight from DIF/VIF entries.
 *   • Old stock (TPL-CFG = 0x1006) — total volume is XOR-obfuscated
 *     against a 16-row de-obfuscation table indexed by frame[0xb] & 0x0F,
 *     and the meter datetime is BCD packed at frame offset 0x1C.
 *
 * The dispatch logic mirrors wmbusmeters: try the obfuscated path first,
 * fall back to the OMS walker if neither marker matches. The output
 * column count is kept short to fit the 19-char value column. */

#include "../../engine/driver.h"
#include "../../_oms_split.h"

#include <stdio.h>
#include <stdint.h>

/* Wmbusmeters' 16×6 de-obfuscation table — copied verbatim. The first
 * dimension is `frame[0xb] & 0x0F`; the six bytes XOR with the six
 * raw-volume bytes at frame[0xf..0x14]. */
static const uint8_t k_xor[16][6] = {
    { 117, 150, 122,  16,  26,  10 }, {  91, 127, 112,  19,  34,  19 },
    { 179,  24, 185,  11, 142, 153 }, { 142, 125, 121,   7,  74,  22 },
    { 181, 145,   7, 154, 203, 105 }, { 184, 163,  50, 161,  57,  14 },
    { 189, 128, 156, 126,  96, 153 }, {  39,  92, 180, 196, 128, 163 },
    {  48, 208,  10, 206,  25,   3 }, { 194,  76, 240,   5, 165, 134 },
    {  84,  75,  22, 152,  17,  94 }, {  75, 238,  12, 201, 125, 162 },
    { 135, 202,  74,  72, 228,  31 }, { 196, 135, 119,  46, 138, 232 },
    { 227,  48, 189, 120,  87, 140 }, { 164, 154,  57, 111,  40,   5 }
};

static int bcd2bin(uint8_t b) {
    return ((b >> 4) & 0x0F) * 10 + (b & 0x0F);
}

static size_t decode_rfmtx1(const WmbusDecodeCtx* ctx, char* out, size_t cap) {
    const uint8_t* a = ctx->apdu;
    size_t l = ctx->apdu_len;

    /* `tpl_cfg == 0x1006` triggers the legacy XOR path. The OMS short
     * header lays out as ACC[0] ST[1] CW_lo[2] CW_hi[3], so tpl_cfg is
     * just a 16-bit LE pull from apdu[2..3]. */
    bool legacy = false;
    if(l >= 4) {
        uint16_t cfg = (uint16_t)a[2] | ((uint16_t)a[3] << 8);
        if(cfg == 0x1006) legacy = true;
    }

    /* Wmbusmeters numbers offsets against the full link frame, where CI
     * sits at frame[10]. Our `apdu` already starts at frame[11], so each
     * upstream index drops by 11: frame[0xB] -> apdu[0],
     * frame[0xF..0x14] -> apdu[4..9], frame[28..33] -> apdu[17..22]. */
    if(legacy && l >= 23) {
        size_t pos = (size_t)snprintf(out, cap, "Sontex RFM-TX1\n");
        uint8_t  k_idx = a[0] & 0x0F;
        uint8_t  base  = a[0];
        uint8_t dec[6];
        for(int i = 0; i < 6; i++) dec[i] = a[4 + i] ^ base ^ k_xor[k_idx][i];

        /* dec[0..1] carry status flags; the volume is BCD-packed across
         * dec[2..5] with each byte being the next *100 decade. */
        double total = (double)0.0, mul = (double)1.0;
        for(int i = 2; i < 6; i++) {
            total += mul * (double)bcd2bin(dec[i]);
            mul *= (double)100.0;
        }
        total /= (double)1000.0;
        pos += (size_t)snprintf(out + pos, cap - pos, "Total  %.3f m3\n", total);

        /* Datetime: BCD seconds/minutes/hour/day/month/year. */
        if(l >= 23) {
            int s  = bcd2bin(a[17]), mn = bcd2bin(a[18]), h = bcd2bin(a[19]);
            int d  = bcd2bin(a[20]), mo = bcd2bin(a[21]);
            int y  = 2000 + bcd2bin(a[22]);
            pos += (size_t)snprintf(out + pos, cap - pos,
                                    "Time  %04d-%02d-%02d %02d:%02d\n",
                                    y, mo % 13, d % 32, h % 24, mn % 60);
            (void)s;
        }
        return pos;
    }

    /* Default path: walk the OMS portion. New-stock RFM-TX1 ships valid
     * DIF/VIF entries that already decode total + datetime. */
    return wmbus_engine_render_oms("Sontex RFM-TX1", a, l, out, cap);
}

static const WmbusMVT k_mvt[] = {
    {"BMT", 0x07, 0x05},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_sontex_rfmtx1 = {
    .id = "rfmtx1", .title = "Sontex RFM-TX1",
    .mvt = k_mvt, .decode_ex = decode_rfmtx1,
};
