/* Techem Vario 4 Typ 4.5.1 (heat) — wmbusmeters `src/driver_vario451.cc`.
 *
 * Same byte offsets as compact5 but 16-bit prev/curr fields whose
 * raw value is "GJ × 1000". The decoder reports both kWh and GJ. */

#include "../../engine/driver.h"
#include "_compact.h"

/* (version, type) per wmbusmeters `drivers/src/vario451.xmq`. */
static const WmbusMVT k_mvt[] = {{"TCH", 0x27, 0x04}, {"TCH", 0x27, 0xC3}, {{0}, 0, 0}};

static size_t
    decode(uint16_t m, uint8_t v, uint8_t med, const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m;
    (void)v;
    (void)med;
    return techem_decode_vario(a, l, o, cap);
}

const WmbusDriver wmbus_drv_techem_vario = {
    .id = "techem-vario",
    .title = "Techem Vario heat",
    .mvt = k_mvt,
    .decode = decode,
};

/* Vario 451 MID (0x17) and Vario 411 (0x28) send standard OMS
 * DIF/VIF records (wmbusmeters `vario451mid.xmq`, `vario411.xmq`),
 * not the compact layout, so they use the generic walker. */
static const WmbusMVT k_mvt_oms[] = {{"TCH", 0x17, 0x04}, {"TCH", 0x28, 0x04}, {{0}, 0, 0}};
const WmbusDriver wmbus_drv_techem_vario_oms = {
    .id = "techem-vario-oms",
    .title = "Techem Vario heat",
    .mvt = k_mvt_oms,
    .decode = NULL,
};
