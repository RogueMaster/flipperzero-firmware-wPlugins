/* Shared 12-byte HCA preamble used by Qundis, ista and Brunata.
 *
 * Reverse-engineered from the open-source wmbusmeters drivers:
 *   _refs/wmbusmeters/drivers/src/qcaloric.xmq   (Qundis Q-Caloric)
 *   _refs/wmbusmeters/src/                       (ista doprimo, Brunata Optuna — declarative)
 *
 * On-air layout (after the manufacturer-specific CI byte):
 *
 *   off  size  field
 *   0    1     subtype / control byte (0x09 = current cycle reading)
 *   1    1     status / extension
 *   2    2     current period reading  (LE u16, dimensionless units)
 *   4    2     previous period reading (LE u16)
 *   6    2     end-of-current-period date  (M-bus type G)
 *   8    2     end-of-previous-period date
 *   10   1     room temperature, signed (× 0.5 °C)  [optional]
 *   11   1     radiator temperature, signed (× 0.5 °C) [optional]
 */

#include "../../engine/driver.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

static int put(char* o, size_t cap, size_t pos, const char* fmt, ...) {
    if(pos >= cap) return 0;
    va_list ap;
    __builtin_va_start(ap, fmt);
    int n = vsnprintf(o + pos, cap - pos, fmt, ap);
    __builtin_va_end(ap);
    return n < 0 ? 0 : n;
}

/* M-Bus type-G date: 16-bit little-endian. */
static void date_g(uint16_t w, char* out, size_t cap) {
    int day = w & 0x1F;
    int mon = (w >> 8) & 0x0F;
    int yr  = ((w >> 5) & 0x07) | (((w >> 12) & 0x0F) << 3);
    if(!day || !mon || mon > 12) snprintf(out, cap, "-");
    else snprintf(out, cap, "%04d-%02d-%02d", 2000 + yr, mon, day);
}

static size_t render_hca(const char* title, const uint8_t* a, size_t l,
                         char* o, size_t cap) {
    size_t pos = 0;
    if(l < 12) {
        pos += put(o, cap, pos, "%s\n", title);
        pos += put(o, cap, pos, "Bytes  %u\n", (unsigned)l);
        size_t n = l < 16 ? l : 16;
        pos += put(o, cap, pos, "Hex    ");
        for(size_t i = 0; i < n; i++) pos += put(o, cap, pos, "%02X", a[i]);
        if(l > n) pos += put(o, cap, pos, "..");
        pos += put(o, cap, pos, "\n");
        return pos;
    }
    uint16_t cur  = (uint16_t)(a[2] | (a[3] << 8));
    uint16_t prev = (uint16_t)(a[4] | (a[5] << 8));
    uint16_t d1   = (uint16_t)(a[6] | (a[7] << 8));
    uint16_t d2   = (uint16_t)(a[8] | (a[9] << 8));
    int8_t   tr   = (int8_t)a[10];
    int8_t   th   = (int8_t)a[11];
    char ds1[16], ds2[16];
    date_g(d1, ds1, sizeof(ds1));
    date_g(d2, ds2, sizeof(ds2));
    pos += put(o, cap, pos, "%s\n", title);
    pos += put(o, cap, pos, "Now    %u u\n", cur);
    pos += put(o, cap, pos, "Last   %u u\n", prev);
    pos += put(o, cap, pos, "Period %s\n", ds1);
    pos += put(o, cap, pos, "Prev   %s\n", ds2);
    if(tr > -40 && tr < 120)
        pos += put(o, cap, pos, "Room   %d.%d C\n",
                   tr / 2, (tr < 0 ? -tr : tr) % 2 ? 5 : 0);
    if(th > -40)  /* int8_t max is 127, so the upper bound is implicit */
        pos += put(o, cap, pos, "Rad    %d.%d C\n",
                   th / 2, (th < 0 ? -th : th) % 2 ? 5 : 0);
    return pos;
}

/* ---- Qundis Q-Caloric ---- */
static const WmbusMVT k_mvt_qds[] = {
    {"QDS", 0xFF, 0x08}, {"QDS", 0xFF, 0x04},
    {"LSE", 0x18, 0x08}, {"LSE", 0x34, 0x08}, {"LSE", 0x35, 0x08},
    {"ZRI", 0xFD, 0x08}, {"ZRI", 0xFC, 0x08},
    { {0}, 0, 0 }
};
static size_t decode_qds(uint16_t m, uint8_t v, uint8_t med,
                         const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m; (void)v; (void)med;
    return render_hca("Qundis HCA", a, l, o, cap);
}
const WmbusDriver wmbus_drv_qundis_hca = {
    .id = "qundis-hca",
    .title = "Qundis HCA",
    .mvt = k_mvt_qds,
    .decode = decode_qds,
};

/* ---- ista doprimo / istameter ---- */
static const WmbusMVT k_mvt_ist[] = {
    {"IST", 0xFF, 0x08},
    {"IST", 0xa9, 0x04},
    { {0}, 0, 0 }
};
static size_t decode_ist(uint16_t m, uint8_t v, uint8_t med,
                         const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m; (void)v; (void)med;
    return render_hca("ista HCA", a, l, o, cap);
}
const WmbusDriver wmbus_drv_ista_hca = {
    .id = "ista-hca",
    .title = "ista doprimo HCA",
    .mvt = k_mvt_ist,
    .decode = decode_ist,
};

/* ---- Brunata Futura+ ---- */
static const WmbusMVT k_mvt_bra[] = {
    {"BRA", 0xFF, 0x08},
    { {0}, 0, 0 }
};
static size_t decode_bra(uint16_t m, uint8_t v, uint8_t med,
                         const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m; (void)v; (void)med;
    return render_hca("Brunata HCA", a, l, o, cap);
}
const WmbusDriver wmbus_drv_brunata_hca = {
    .id = "brunata-hca",
    .title = "Brunata Futura+ HCA",
    .mvt = k_mvt_bra,
    .decode = decode_bra,
};
