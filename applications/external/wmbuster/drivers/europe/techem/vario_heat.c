/* Techem Vario 4 Typ 4.5.1 (heat) — wmbusmeters `src/driver_vario451.cc`.
 *
 * Same byte offsets as compact5 but 16-bit prev/curr fields whose
 * raw value is "GJ × 1000". The decoder reports both kWh and GJ. */

#include "../../engine/driver.h"
#include "_compact.h"

static const WmbusMVT k_mvt[] = {
    {"TCH", 0x04, 0x27},
    {"TCH", 0xC3, 0x27},
    /* The MID-certified Vario 451 ships under (TCH, 0x17, 0x04)
     * per the wmbusmeters declarative driver `vario451mid.xmq`. */
    {"TCH", 0x17, 0x04},
    /* Vario 411 (older heat meter) — ver 0x28, med 0x04. The
     * billing-period offsets are identical, so the Vario decoder
     * applies. */
    {"TCH", 0x28, 0x04},
    { {0}, 0, 0 }
};

static size_t decode(uint16_t m, uint8_t v, uint8_t med,
                     const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m; (void)v; (void)med;
    return techem_decode_vario(a, l, o, cap);
}

const WmbusDriver wmbus_drv_techem_vario = {
    .id = "techem-vario",
    .title = "Techem Vario heat",
    .mvt = k_mvt,
    .decode = decode,
};
