/* Sontex — 868 HCA, Supercal heat, Supercom 587 water.
 * wmbusmeters: drivers/src/{sontex868,supercal,supercom587}.xmq. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt_868[] = {
    {"SON", 0x16, 0x08},
    {"SON", 0xFF, 0x08},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_sontex_868 = {
    .id = "sontex-868", .title = "Sontex 868 HCA",
    .mvt = k_mvt_868, .decode = NULL,
};

static const WmbusMVT k_mvt_supercal[] = {
    {"SON", 0x1B, 0x04},
    {"SON", 0xFF, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_sontex_supercal = {
    .id = "sontex-supercal", .title = "Sontex Supercal heat",
    .mvt = k_mvt_supercal, .decode = NULL,
};

static const WmbusMVT k_mvt_supercom[] = {
    {"SON", 0x3C, 0x06},
    {"SON", 0x3C, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_sontex_supercom = {
    .id = "sontex-supercom", .title = "Sontex Supercom 587",
    .mvt = k_mvt_supercom, .decode = NULL,
};
