/* Techem FHKV-III / FHKV-IV heat-cost allocator.
 *
 * MVT triples per wmbusmeters (`drivers/src/fhkvdataiv.xmq` and
 * `src/driver_fhkvdataiii.cc`). The `version` byte selects the
 * temperature offset inside the payload — see _compact.c. */

#include "../../engine/driver.h"
#include "_compact.h"

static const WmbusMVT k_mvt[] = {
    /* (M, V, T) — version + medium as carried in the link header.
     * Techem uses medium 0x80 in compact frames; some firmware
     * versions also send the OMS-canonical 0x08. */
    {"TCH", 0x69, 0x08},
    {"TCH", 0x69, 0x80},
    {"TCH", 0x94, 0x08},
    {"TCH", 0x94, 0x80},
    /* Some Techem firmwares ship the version / medium bytes swapped in
     * the link header — wmbusmeters lists both orderings under the same
     * `fhkvdataiii` driver. */
    {"TCH", 0x80, 0x69},
    {"TCH", 0x80, 0x94},
    { {0}, 0, 0 }
};

static size_t decode(uint16_t m, uint8_t v, uint8_t med,
                     const uint8_t* a, size_t l, char* o, size_t cap) {
    (void)m; (void)med;
    return techem_decode_fhkv(v, a, l, o, cap);
}

const WmbusDriver wmbus_drv_techem_fhkv = {
    .id = "techem-fhkv",
    .title = "Techem FHKV HCA",
    .mvt = k_mvt,
    .decode = decode,
};
