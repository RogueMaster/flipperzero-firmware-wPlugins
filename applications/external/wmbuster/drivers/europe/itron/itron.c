/* Itron / Actaris — BM+M / Aquadis water, gas and heat meters.
 * wmbusmeters: drivers/src/{itron,itronheat}.xmq. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt_itron[] = {
    {"ITW", 0x00, 0x07},
    {"ITW", 0x03, 0x07},
    {"ITW", 0x33, 0x07},
    {"ITW", 0x00, 0x16},
    {"ITW", 0xFF, 0x07},
    {"ITW", 0xFF, 0x16},
    /* Actaris-rebadged water meter. */
    {"ACW", 0xFF, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_itron_water = {
    .id = "itron-water", .title = "Itron BM+M water",
    .mvt = k_mvt_itron, .decode = NULL,
};

static const WmbusMVT k_mvt_itron_heat[] = {
    {"ITW", 0x00, 0x04},
    {"ITW", 0xFF, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_itron_heat = {
    .id = "itron-heat", .title = "Itron heat",
    .mvt = k_mvt_itron_heat, .decode = NULL,
};

static const WmbusMVT k_mvt_itron_gas[] = {
    {"ITW", 0xFF, 0x03},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_itron_gas = {
    .id = "itron-gas", .title = "Itron gas",
    .mvt = k_mvt_itron_gas, .decode = NULL,
};
