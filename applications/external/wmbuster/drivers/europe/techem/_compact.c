/* Shared Techem compact-frame helpers. See `_compact.h` for the
 * layout citations and the per-decoder offset comments. */

#include "_compact.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int append(char* o, size_t cap, size_t pos, const char* fmt, ...) {
    if(pos >= cap) return 0;
    va_list ap;
    __builtin_va_start(ap, fmt);
    int n = vsnprintf(o + pos, cap - pos, fmt, ap);
    __builtin_va_end(ap);
    return n < 0 ? 0 : n;
}

size_t techem_hex_dump(const uint8_t* a, size_t l, char* o, size_t cap,
                       const char* title) {
    size_t pos = 0;
    pos += append(o, cap, pos, "%s\n", title);
    pos += append(o, cap, pos, "Bytes  %u\n", (unsigned)l);
    size_t off = 0;
    int rows = 0;
    while(off < l && rows < 6 && pos + 28 < cap) {
        size_t n = l - off; if(n > 8) n = 8;
        pos += append(o, cap, pos, "Hex%02u  ", (unsigned)off);
        for(size_t k = 0; k < n; k++)
            pos += append(o, cap, pos, "%02X", a[off + k]);
        pos += append(o, cap, pos, "\n");
        off += n; rows++;
    }
    return pos;
}

/* Techem-private "previous" date packing: bits 0..4 day,
 * 5..8 month, 9..14 year (year is years-since-2000). */
static void techem_prev_date(uint16_t w, char* out, size_t cap, int* yr_out) {
    int day = w & 0x1F;
    int mon = (w >> 5) & 0x0F;
    int yr  = 2000 + ((w >> 9) & 0x3F);
    if(yr_out) *yr_out = yr;
    if(!day || !mon || mon > 12) snprintf(out, cap, "-");
    else snprintf(out, cap, "%04d-%02d-%02d", yr, mon, day);
}

/* Techem-private "current" date packing: bits 4..8 day, 9..12
 * month. The year is derived from the previous-period year by
 * adding 1 if the current month falls before the previous one
 * (or matches with an earlier day) — see fhkvdataiii.cc:131-148. */
static void techem_curr_date(uint16_t w, int yr_prev,
                             int day_prev, int mon_prev,
                             char* out, size_t cap) {
    int day = (w >> 4) & 0x1F;
    int mon = (w >> 9) & 0x0F;
    int yr  = yr_prev;
    if(mon < mon_prev || (mon == mon_prev && day <= day_prev)) yr++;
    if(!day || !mon || mon > 12) snprintf(out, cap, "-");
    else snprintf(out, cap, "%04d-%02d-%02d", yr, mon, day);
}

size_t techem_decode_fhkv(uint8_t version,
                          const uint8_t* a, size_t l,
                          char* o, size_t cap) {
    if(l < 13) return techem_hex_dump(a, l, o, cap, "Techem FHKV short");

    uint16_t prev_date_w = (uint16_t)(a[1] | (a[2] << 8));
    uint16_t prev_val    = (uint16_t)(a[3] | (a[4] << 8));
    uint16_t curr_date_w = (uint16_t)(a[5] | (a[6] << 8));
    uint16_t curr_val    = (uint16_t)(a[7] | (a[8] << 8));

    int yr_prev = 0;
    int day_prev = prev_date_w & 0x1F;
    int mon_prev = (prev_date_w >> 5) & 0x0F;
    char dprev[16], dcurr[16];
    techem_prev_date(prev_date_w, dprev, sizeof(dprev), &yr_prev);
    techem_curr_date(curr_date_w, yr_prev, day_prev, mon_prev,
                     dcurr, sizeof(dcurr));

    /* Temperatures: u16 / 100 °C. Offset shifts by version. */
    int troom = -32768, trad = -32768;
    size_t off = (version == 0x94) ? 10 : 9;
    if(l >= off + 4) {
        int t_room = (int)((uint16_t)(a[off]     | (a[off + 1] << 8)));
        int t_rad  = (int)((uint16_t)(a[off + 2] | (a[off + 3] << 8)));
        if(t_room >= 0 && t_room <= 9000) troom = t_room;
        if(t_rad  >= 0 && t_rad  <= 9000) trad  = t_rad;
    }

    size_t pos = 0;
    pos += append(o, cap, pos, "Techem FHKV HCA\n");
    pos += append(o, cap, pos, "Now    %u u\n",  curr_val);
    pos += append(o, cap, pos, "Prev   %u u\n",  prev_val);
    pos += append(o, cap, pos, "Reset  %s\n",    dprev);
    pos += append(o, cap, pos, "Ends   %s\n",    dcurr);
    if(troom != -32768)
        pos += append(o, cap, pos, "Room   %d.%02d C\n",
                      troom / 100, (troom < 0 ? -troom : troom) % 100);
    if(trad != -32768)
        pos += append(o, cap, pos, "Rad    %d.%02d C\n",
                      trad / 100, (trad < 0 ? -trad : trad) % 100);
    return pos;
}

size_t techem_decode_water(bool with_dates,
                           const uint8_t* a, size_t l,
                           char* o, size_t cap) {
    if(l < 9) return techem_hex_dump(a, l, o, cap, "Techem water short");
    uint16_t prev = (uint16_t)(a[3] | (a[4] << 8));    /* deci-m^3 */
    uint16_t curr = (uint16_t)(a[7] | (a[8] << 8));
    uint32_t total_dl = (uint32_t)prev + curr;

    size_t pos = 0;
    pos += append(o, cap, pos, "Techem water\n");
    pos += append(o, cap, pos, "Now    %lu.%lu m3\n",
                  (unsigned long)(total_dl / 10), (unsigned long)(total_dl % 10));
    pos += append(o, cap, pos, "Period %u.%u m3\n",
                  curr / 10, curr % 10);
    pos += append(o, cap, pos, "Last   %u.%u m3\n",
                  prev / 10, prev % 10);
    if(with_dates) {
        uint16_t prev_date_w = (uint16_t)(a[1] | (a[2] << 8));
        uint16_t curr_date_w = (uint16_t)(a[5] | (a[6] << 8));
        int yr_prev = 0;
        int day_prev = prev_date_w & 0x1F;
        int mon_prev = (prev_date_w >> 5) & 0x0F;
        char dprev[16], dcurr[16];
        techem_prev_date(prev_date_w, dprev, sizeof(dprev), &yr_prev);
        techem_curr_date(curr_date_w, yr_prev, day_prev, mon_prev,
                         dcurr, sizeof(dcurr));
        pos += append(o, cap, pos, "Reset  %s\n", dprev);
        pos += append(o, cap, pos, "Ends   %s\n", dcurr);
    }
    return pos;
}

size_t techem_decode_compact5(const uint8_t* a, size_t l,
                              char* o, size_t cap) {
    if(l < 10) return techem_hex_dump(a, l, o, cap, "Techem heat short");
    /* u24 little-endian prev/curr at offsets 3..5 and 7..9. */
    uint32_t prev = (uint32_t)(a[3] | (a[4] << 8) | (a[5] << 16));
    uint32_t curr = (uint32_t)(a[7] | (a[8] << 8) | (a[9] << 16));
    uint32_t total = prev + curr;
    size_t pos = 0;
    pos += append(o, cap, pos, "Techem compact heat\n");
    pos += append(o, cap, pos, "Total  %lu kWh\n", (unsigned long)total);
    pos += append(o, cap, pos, "Period %lu kWh\n", (unsigned long)curr);
    pos += append(o, cap, pos, "Last   %lu kWh\n", (unsigned long)prev);
    return pos;
}

size_t techem_decode_vario(const uint8_t* a, size_t l,
                           char* o, size_t cap) {
    if(l < 9) return techem_hex_dump(a, l, o, cap, "Vario heat short");
    /* Raw u16 = "GJ × 1000". Convert to kWh via integer math:
     *   1 GJ = 1e9 J = 1e9 / 3600 Wh ≈ 277.778 kWh
     *   raw / 1000 GJ × (1000 × 1000 / 3600) kWh/GJ
     *  = raw × (1000 / 3600) / 1
     *  = raw × 250 / 9    (exact rational form)
     * Width: raw ≤ 65535 → kWh ≤ 1820972. Fits in u32. */
    uint16_t prev_raw = (uint16_t)(a[3] | (a[4] << 8));
    uint16_t curr_raw = (uint16_t)(a[7] | (a[8] << 8));
    uint32_t prev_kwh = (uint32_t)((uint64_t)prev_raw * 250 / 9);
    uint32_t curr_kwh = (uint32_t)((uint64_t)curr_raw * 250 / 9);

    /* GJ to two decimals: raw / 1000 GJ → raw / 10 in centi-GJ. */
    uint32_t prev_gj_x100 = (uint32_t)prev_raw / 10;
    uint32_t curr_gj_x100 = (uint32_t)curr_raw / 10;

    size_t pos = 0;
    pos += append(o, cap, pos, "Techem Vario heat\n");
    pos += append(o, cap, pos, "Total  %lu kWh\n",
                  (unsigned long)(prev_kwh + curr_kwh));
    pos += append(o, cap, pos, "TotGJ  %lu.%02lu\n",
                  (unsigned long)((prev_gj_x100 + curr_gj_x100) / 100),
                  (unsigned long)((prev_gj_x100 + curr_gj_x100) % 100));
    pos += append(o, cap, pos, "Period %lu kWh\n", (unsigned long)curr_kwh);
    pos += append(o, cap, pos, "PerGJ  %lu.%02lu\n",
                  (unsigned long)(curr_gj_x100 / 100),
                  (unsigned long)(curr_gj_x100 % 100));
    pos += append(o, cap, pos, "Last   %lu kWh\n", (unsigned long)prev_kwh);
    pos += append(o, cap, pos, "LastGJ %lu.%02lu\n",
                  (unsigned long)(prev_gj_x100 / 100),
                  (unsigned long)(prev_gj_x100 % 100));
    return pos;
}

size_t techem_decode_smoke(const uint8_t* a, size_t l,
                           char* o, size_t cap) {
    size_t pos = 0;
    pos += append(o, cap, pos, "Techem smoke\n");
    if(l > 0) pos += append(o, cap, pos, "Status 0x%02X\n", a[0]);
    if(l > 1) pos += append(o, cap, pos, "Flags  0x%02X\n", a[1]);
    if(l > 2) pos += append(o, cap, pos, "Bytes  %u\n", (unsigned)l);
    return pos;
}
