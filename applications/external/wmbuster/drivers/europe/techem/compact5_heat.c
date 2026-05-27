/* Techem Compact V (heat) — wmbusmeters `src/driver_compact5.cc`.
 *
 * 24-bit prev/curr energy in kWh, no date fields. */

#include "../../engine/driver.h"
#include "_compact.h"

static const WmbusMVT k_mvt[] = {
    {"TCH", 0x04, 0x45},
    {"TCH", 0xC3, 0x45},
    {"TCH", 0x43, 0x45},
    {"TCH", 0x43, 0x39},
    {"TCH", 0x43, 0x22},
    { {0}, 0, 0 }
};

static size_t decode(uint16_t m, uint8_t v, uint8_t med,
                     const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m; (void)v; (void)med;
    return techem_decode_compact5(a, l, o, cap);
}

const WmbusDriver wmbus_drv_techem_compact5 = {
    .id = "techem-compact5",
    .title = "Techem compact5 heat",
    .mvt = k_mvt,
    .decode = decode,
};
