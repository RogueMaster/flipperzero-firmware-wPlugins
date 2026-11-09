/* Techem smoke detector. The full alarm protocol is not public;
 * this driver surfaces the status/flag bytes verbatim and lets
 * the user spot a non-zero alarm condition by eye. */

#include "../../engine/driver.h"
#include "_compact.h"

static const WmbusMVT k_mvt[] = {
    /* Wildcard version, OMS smoke-detector type 0x1A. */
    {"TCH", 0xFF, 0x1A},
    {{0}, 0, 0}};

static size_t
    decode(uint16_t m, uint8_t v, uint8_t med, const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m;
    (void)v;
    (void)med;
    return techem_decode_smoke(a, l, o, cap);
}

const WmbusDriver wmbus_drv_techem_smoke = {
    .id = "techem-smoke",
    .title = "Techem smoke",
    .mvt = k_mvt,
    .decode = decode,
};

/* TSD2 — wmbusmeters `drivers/src/tsd2.xmq`: version 0x76, type 0xF0.
 * Payload [0] status (0x00 OK, 0x01 smoke, 0x80 defect),
 * [1..2] previous billing date. */
static const WmbusMVT k_mvt_tsd2[] = {{"TCH", 0x76, 0xF0}, {{0}, 0, 0}};
static size_t decode_tsd2(
    uint16_t m,
    uint8_t v,
    uint8_t med,
    const uint8_t* a,
    size_t l,
    char* o,
    size_t cap) {
    (void)m;
    (void)v;
    (void)med;
    return techem_decode_tsd2(a, l, o, cap);
}
const WmbusDriver wmbus_drv_techem_tsd2 = {
    .id = "techem-tsd2",
    .title = "Techem TSD2 smoke",
    .mvt = k_mvt_tsd2,
    .decode = decode_tsd2,
};
