/* Maddalena (Italy) — Microclima heat, EVO868 water, generic water.
 * wmbusmeters: drivers/src/{maddalena,microclima,evo868}.xmq. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt_water[] = {
    {"MAD", 0x01, 0x06},
    {"MAD", 0x01, 0x07},
    {"MAD", 0xFF, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_maddalena_water = {
    .id = "maddalena-water", .title = "Maddalena water",
    .mvt = k_mvt_water, .decode = NULL,
};

static const WmbusMVT k_mvt_microclima[] = {
    {"MAD", 0x00, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_maddalena_microclima = {
    .id = "microclima", .title = "Maddalena Microclima",
    .mvt = k_mvt_microclima, .decode = NULL,
};

static const WmbusMVT k_mvt_evo868[] = {
    {"MAD", 0x50, 0x06},
    {"MAD", 0x50, 0x07},
    {"MAD", 0x50, 0x16},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_maddalena_evo868 = {
    .id = "evo868", .title = "Maddalena EVO868",
    .mvt = k_mvt_evo868, .decode = NULL,
};
