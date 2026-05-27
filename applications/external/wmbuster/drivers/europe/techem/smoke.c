/* Techem smoke detector. The full alarm protocol is not public;
 * this driver surfaces the status/flag bytes verbatim and lets
 * the user spot a non-zero alarm condition by eye. */

#include "../../engine/driver.h"
#include "_compact.h"

static const WmbusMVT k_mvt[] = {
    /* Wildcard medium 0x1A covers OMS-canonical smoke-detector
     * frames; the second tuple is the Techem-private medium. */
    {"TCH", 0xFF, 0x1A},
    {"TCH", 0x00, 0x76},   /* legacy code seen in older Techem stock */
    {"TCH", 0xF0, 0x76},   /* TSD2 — wmbusmeters tsd2 driver        */
    { {0}, 0, 0 }
};

static size_t decode(uint16_t m, uint8_t v, uint8_t med,
                     const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m; (void)v; (void)med;
    return techem_decode_smoke(a, l, o, cap);
}

const WmbusDriver wmbus_drv_techem_smoke = {
    .id = "techem-smoke",
    .title = "Techem smoke",
    .mvt = k_mvt,
    .decode = decode,
};
