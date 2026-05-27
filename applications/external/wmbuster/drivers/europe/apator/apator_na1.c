/* Apator NA-1 water meter (Polish water meter, popular in PL utility
 * networks). Port stub for `_refs/wmbusmeters/src/driver_apatorna1.cc`.
 *
 * The NA-1 ships frames encrypted with AES-CBC using a *per-meter key*
 * supplied by the utility. wmbusmeters' upstream code calls the
 * "decrypt_TPL_AES_CBC_IV" helper with whatever key is configured —
 * meaning it works only when the operator has loaded the meter's key
 * file. Without that key the bytes are random.
 *
 * Our existing `wmbus_app.c` already attempts mode-5 decryption against
 * `keys.csv` for any frame whose CI byte is in the standard encrypted
 * set. NA-1 uses CI=0xA0 which is *not* in that set, so the frame
 * currently flows here unencrypted-but-scrambled.
 *
 * Until we extend the key path to cover this CI we expose a friendly
 * "key required" message via the standard fallback formatting. The
 * MVT registration ensures the meter shows up with the correct title
 * on the scan list and the OK-toggle hex view still works. */

#include "../../engine/driver.h"

#include <stdio.h>
#include <stdint.h>

static size_t decode_na1(uint16_t m, uint8_t v, uint8_t med,
                         const uint8_t* a, size_t l,
                         char* o, size_t cap) {
    (void)m; (void)v; (void)med; (void)a;
    size_t pos = (size_t)snprintf(o, cap, "Apator NA-1 water\n");
    pos += (size_t)snprintf(o + pos, cap - pos,
                            "Bytes  %u\n"
                            "Status  encrypted\n"
                            "Need  per-meter key\n",
                            (unsigned)l);
    return pos;
}

static const WmbusMVT k_mvt[] = {
    {"APA", 0x07, 0x14},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_na1 = {
    .id = "apatorna1", .title = "Apator NA-1 water",
    .mvt = k_mvt, .decode = decode_na1,
};
