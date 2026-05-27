/* B+W (BFW Bochum) 240-Radio heat-cost allocator — port of
 * `_refs/wmbusmeters/src/driver_bfw240radio.cc`.
 *
 * Frame layout (after the BFW link header, contents passed to us as
 * `apdu`): 40 bytes carrying current + previous + 18 historic monthly
 * HCA values plus a BCD device date.
 *
 * The custom encoding is awkward — historic values share nibbles with
 * neighbours so we extract them with the bit-twiddle in `historic()`
 * (mirroring wmbusmeters byte-for-byte). Layout, copied from the
 * upstream comment for sanity:
 *
 *     [0..3]   filler 2F2F + frame mark + ???
 *     [4..5]   prev_hca       (LE u16)
 *     [6..7]   current_hca    (LE u16)
 *     [8..35]  18 packed 12-bit historic values
 *     [37..39] device date in BCD (DD MM YY)
 *
 * The screen budget on a 128×64 Flipper makes 18 historic rows
 * impractical, so we surface current + prev + date as labels and roll
 * the historic series into an "H1..H4" preview row — the full series
 * is always available via the OK→raw toggle. */

#include "../../engine/driver.h"

#include <stdio.h>
#include <stdint.h>

static int historic(int n, const uint8_t* c) {
    /* Mirror of wmbusmeters' getHistoric(): packed 12-bit BE values
     * counted from the *end* of the buffer at index 36. */
    int offset = (n * 12) / 8;
    int remainder = (n * 12) % 8;
    uint8_t lo, hi;
    if(remainder == 0) {
        lo = c[36 - offset];
        hi = 0x0F & c[36 - 1 - offset];
    } else {
        lo = c[36 - 1 - offset];
        hi = (0xF0 & c[36 - offset]) >> 4;
    }
    return (int)hi * 256 + lo;
}

static size_t decode_bfw(uint16_t m, uint8_t v, uint8_t med,
                         const uint8_t* a, size_t l,
                         char* o, size_t cap) {
    (void)m; (void)v; (void)med;
    size_t pos = (size_t)snprintf(o, cap, "BFW 240-Radio HCA\n");
    if(l < 40) {
        pos += (size_t)snprintf(o + pos, cap - pos,
                                "Bytes  %u\n"
                                "Status  short frame\n", (unsigned)l);
        return pos;
    }
    int prev    = (int)a[4]  * 256 + a[5];
    int current = (int)a[6]  * 256 + a[7];

    /* Date BCD bytes are little-endian DD MM YY at offsets 37..39. */
    pos += (size_t)snprintf(o + pos, cap - pos,
                            "Now  %d HCA\n"
                            "Prev mon  %d HCA\n"
                            "Date  20%02X-%02X-%02X\n",
                            current, prev,
                            (unsigned)a[39],     /* YY */
                            (unsigned)a[38],     /* MM */
                            (unsigned)a[37]);    /* DD */

    /* Quick preview of the four most recent historic months — keeps the
     * screen useful while still hinting at the deeper data set. The full
     * 18-month series is recoverable from the raw-bytes view. */
    pos += (size_t)snprintf(o + pos, cap - pos,
                            "H1  %d\nH2  %d\nH3  %d\nH4  %d\n",
                            historic(0, a), historic(1, a),
                            historic(2, a), historic(3, a));
    return pos;
}

/* Replace the placeholder list previously in misc.c — we now have a real
 * decoder, so register the driver here and wire it through registry.c. */
static const WmbusMVT k_mvt[] = {
    {"BFW", 0x08, 0x02},   /* canonical wmbusmeters tuple              */
    {"BFW", 0xFF, 0x08},   /* future-proof: any BFW HCA medium 0x08   */
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_bfw_radio = {
    .id = "bfw-240radio", .title = "BFW 240-Radio HCA",
    .mvt = k_mvt, .decode = decode_bfw,
};
