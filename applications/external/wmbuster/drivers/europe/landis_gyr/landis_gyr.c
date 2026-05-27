/* Landis+Gyr — Ultraheat T550 / UH50 heat meters and the
 * LSE-coded L+S meters (Landis+Staefa, the Siemens-era brand).
 * wmbusmeters refs: drivers/src/{ultraheat,lse_07_17,lse_08,
 *                                  qheat_55_us}.xmq. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt_ultraheat[] = {
    {"LUG", 0x04, 0x04},
    {"LUG", 0x07, 0x04},
    {"LUG", 0x0A, 0x04},
    {"LUG", 0xFF, 0x04},
    {"LUG", 0xFF, 0x07},
    {"LUG", 0xFF, 0x02},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_lg_ultraheat = {
    .id = "ultraheat", .title = "Landis+Gyr Ultraheat",
    .mvt = k_mvt_ultraheat, .decode = NULL,
};

/* Landis+Staefa water family */
static const WmbusMVT k_mvt_lse[] = {
    {"LSE", 0x16, 0x07},
    {"LSE", 0x17, 0x07},
    {"LSE", 0x18, 0x06},
    {"LSE", 0x18, 0x07},
    {"LSE", 0xD8, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_lse_water = {
    .id = "lse-water", .title = "Landis+Staefa water",
    .mvt = k_mvt_lse, .decode = NULL,
};

/* Q-Heat 5 (LSE 0x23,0x04) */
static const WmbusMVT k_mvt_qheat5[] = {
    {"LSE", 0x23, 0x04},
    {"LSE", 0x01, 0x08},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_qheat5 = {
    .id = "qheat5", .title = "Qundis Q-Heat 5",
    .mvt = k_mvt_qheat5, .decode = NULL,
};

/* Generic LGB / LGZ / LUG fallback */
static const WmbusMVT k_mvt_lg_fallback[] = {
    {"LGB", 0xFF, 0xFF},
    {"LGZ", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_lg_fallback = {
    .id = "landis-gyr", .title = "Landis+Gyr meter",
    .mvt = k_mvt_lg_fallback, .decode = NULL,
};
