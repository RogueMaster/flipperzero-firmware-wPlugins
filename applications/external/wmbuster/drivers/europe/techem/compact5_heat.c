/* Techem Compact V (heat) — wmbusmeters `src/driver_compact5.cc`.
 *
 * 24-bit prev/curr energy in kWh, no date fields. */

#include "../../engine/driver.h"
#include "_compact.h"

/* (version, type) per wmbusmeters `drivers/src/compact5.xmq`. */
static const WmbusMVT k_mvt[] = {
    {"TCH", 0x45, 0x04},
    {"TCH", 0x45, 0xC3},
    {"TCH", 0x22, 0x43},
    {"TCH", 0x45, 0x43},
    {"TCH", 0x39, 0x43},
    {{0}, 0, 0}};

static size_t
    decode(uint16_t m, uint8_t v, uint8_t med, const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m;
    (void)v;
    (void)med;
    return techem_decode_compact5(a, l, o, cap);
}

const WmbusDriver wmbus_drv_techem_compact5 = {
    .id = "techem-compact5",
    .title = "Techem compact5 heat",
    .mvt = k_mvt,
    .decode = decode,
};
