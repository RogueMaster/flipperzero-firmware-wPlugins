/* Qundis Q-Water v2 / Q-Heat v2 / Q-Smoke / Q-Caloric (already
 * covered by hca_compact.c). The non-HCA variants speak OMS so
 * the generic walker handles them.
 * wmbusmeters: drivers/src/{qwaterv2,qheatv2,qsmoke,qheat5us}.xmq. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt_water[] = {
    {"QDS", 0x16, 0x06}, {"QDS", 0x16, 0x07},
    {"QDS", 0x17, 0x06}, {"QDS", 0x17, 0x07},
    {"QDS", 0x18, 0x06}, {"QDS", 0x18, 0x07},
    {"QDS", 0x19, 0x07},
    {"QDS", 0x1A, 0x06}, {"QDS", 0x1A, 0x07},
    {"QDS", 0x1D, 0x06}, {"QDS", 0x1D, 0x07},
    {"QDS", 0x33, 0x37},
    {"QDS", 0x35, 0x37},
    {"QDS", 0x36, 0x06}, {"QDS", 0x36, 0x07},
    {"QDS", 0xFF, 0x07},
    {"QDS", 0xFF, 0x06},
    {"QDS", 0xFF, 0x16},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_qundis_water = {
    .id = "qwaterv2", .title = "Qundis Q-Water",
    .mvt = k_mvt_water, .decode = NULL,
};

static const WmbusMVT k_mvt_heat[] = {
    {"QDS", 0x23, 0x04}, {"QDS", 0x23, 0x37},
    {"QDS", 0x3E, 0x04}, {"QDS", 0x3E, 0x37},
    {"QDS", 0x46, 0x04}, {"QDS", 0x46, 0x37},
    {"QDS", 0x47, 0x04}, {"QDS", 0x47, 0x37},
    {"QDS", 0x48, 0x04}, {"QDS", 0x48, 0x37},
    {"QDS", 0xFF, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_qundis_heat = {
    .id = "qheatv2", .title = "Qundis Q-Heat",
    .mvt = k_mvt_heat, .decode = NULL,
};

static const WmbusMVT k_mvt_smoke[] = {
    {"QDS", 0x21, 0x1A},
    {"QDS", 0x23, 0x1A},
    {"QDS", 0xFF, 0x1A},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_qundis_smoke = {
    .id = "qsmoke", .title = "Qundis Q-Smoke",
    .mvt = k_mvt_smoke, .decode = NULL,
};
