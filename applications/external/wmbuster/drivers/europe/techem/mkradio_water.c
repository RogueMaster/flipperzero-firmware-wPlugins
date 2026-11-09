/* Techem MK-Radio 3 / 4 / 4a water meter.
 *
 * MVT lists mirror wmbusmeters `drivers/src/mkradio3.xmq`,
 * `mkradio4.xmq` and `mkradio4a.xmq`. Their `mvt = TCH,<ver>,<type>`
 * lines are (manufacturer, version, type) — the same order as our
 * WmbusMVT struct. Techem uses type 0x62 for warm water and 0x72 for
 * cold water.
 *
 * Payload (bytes after CI 0xA2):
 *   [0]    status
 *   [1..2] previous billing date (day bits 0-4, month 5-8, year 9-14)
 *   [3..4] volume at previous billing date, 0.1 m3
 *   [5..6] current date (day bits 4-8, month 9-12)
 *   [7..8] volume since previous billing date, 0.1 m3 */

#include "../../engine/driver.h"
#include "_compact.h"

static const char* water_title(uint8_t med) {
    if(med == 0x62) return "Techem warm water";
    if(med == 0x72) return "Techem cold water";
    return "Techem water";
}

/* MK-Radio 3 — version 0x74 (wmbusmeters mkradio3).
 * Version 0x52 is not in wmbusmeters; it was seen in the field with
 * the same layout and valid billing dates, so it is decoded the same way. */
static const WmbusMVT k_mvt_mk3[] = {
    {"TCH", 0x74, 0x62},
    {"TCH", 0x74, 0x72},
    {"TCH", 0x52, 0x62},
    {"TCH", 0x52, 0x72},
    {{0}, 0, 0}};
static size_t
    decode_mk3(uint16_t m, uint8_t v, uint8_t med, const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m;
    (void)v;
    return techem_decode_water(water_title(med), /* with_dates */ true, a, l, o, cap);
}
const WmbusDriver wmbus_drv_techem_mkradio3 = {
    .id = "techem-mkradio3",
    .title = "Techem MK-Radio3 water",
    .mvt = k_mvt_mk3,
    .decode = decode_mk3,
};

/* MK-Radio 4 — versions 0x95 / 0x70 (wmbusmeters mkradio4).
 * wmbusmeters ignores the date bytes, but field captures of 0x95
 * carry valid billing dates, so they are shown (invalid ones render "-"). */
static const WmbusMVT k_mvt_mk4[] = {
    {"TCH", 0x95, 0x62},
    {"TCH", 0x95, 0x72},
    {"TCH", 0x70, 0x62},
    {"TCH", 0x70, 0x72},
    {{0}, 0, 0}};
static size_t
    decode_mk4(uint16_t m, uint8_t v, uint8_t med, const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m;
    (void)v;
    return techem_decode_water(water_title(med), /* with_dates */ true, a, l, o, cap);
}
const WmbusDriver wmbus_drv_techem_mkradio4 = {
    .id = "techem-mkradio4",
    .title = "Techem MK-Radio4 water",
    .mvt = k_mvt_mk4,
    .decode = decode_mk4,
};

/* MK-Radio 4a — wmbusmeters mkradio4a. This one is a standard OMS
 * frame (DIF/VIF), so it goes through the generic walker. */
static const WmbusMVT k_mvt_mk4a[] = {{"TCH", 0x95, 0x37}, {"HYD", 0xFE, 0x06}, {{0}, 0, 0}};
const WmbusDriver wmbus_drv_techem_mkradio4a = {
    .id = "techem-mkradio4a",
    .title = "Techem MK-Radio4a water",
    .mvt = k_mvt_mk4a,
    .decode = NULL,
};
