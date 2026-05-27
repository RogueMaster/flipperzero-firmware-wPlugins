/* Techem MK-Radio 3 / 4 / 4a water meter.
 *
 * MK-Radio 3 telegrams carry billing-period date fields; MK-Radio
 * 4 and 4a omit them (the bytes are zero-filled). MVT lists are
 * mirrored from wmbusmeters `src/driver_mkradio3.cc`,
 * `src/driver_mkradio4.cc`, and `drivers/src/mkradio4a.xmq`. */

#include "../../engine/driver.h"
#include "_compact.h"

/* MK-Radio 3 — version 0x62/0x72 + medium 0x74. */
static const WmbusMVT k_mvt_mk3[] = {
    {"TCH", 0x62, 0x74},
    {"TCH", 0x72, 0x74},
    { {0}, 0, 0 }
};
static size_t decode_mk3(uint16_t m, uint8_t v, uint8_t med,
                         const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m; (void)v; (void)med;
    return techem_decode_water(/* with_dates */ true, a, l, o, cap);
}
const WmbusDriver wmbus_drv_techem_mkradio3 = {
    .id = "techem-mkradio3",
    .title = "Techem MK-Radio3 water",
    .mvt = k_mvt_mk3,
    .decode = decode_mk3,
};

/* MK-Radio 4 / 4a — versions 0x62/0x72/0x95 + mediums 0x70/0x95/0x37. */
static const WmbusMVT k_mvt_mk4[] = {
    {"TCH", 0x62, 0x70},
    {"TCH", 0x72, 0x70},
    {"TCH", 0x62, 0x95},
    {"TCH", 0x72, 0x95},
    {"TCH", 0x95, 0x37},
    {"TCH", 0x72, 0x50},   /* MK Radio 3a — wmbusmeters mkradio3a */
    {"HYD", 0xFE, 0x06},   /* mkradio4a OEM rebadge */
    { {0}, 0, 0 }
};
static size_t decode_mk4(uint16_t m, uint8_t v, uint8_t med,
                         const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m; (void)v; (void)med;
    return techem_decode_water(/* with_dates */ false, a, l, o, cap);
}
const WmbusDriver wmbus_drv_techem_mkradio4 = {
    .id = "techem-mkradio4",
    .title = "Techem MK-Radio4 water",
    .mvt = k_mvt_mk4,
    .decode = decode_mk4,
};
