/* Aquametro — Topas ESKR water + CALEC heat. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt_topas[] = {
    {"AMT", 0xF1, 0x06},
    {"AMT", 0xF1, 0x07},
    {"AMT", 0xFF, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_aquametro_topas = {
    .id = "topaseskr", .title = "Aquametro Topas ESKR",
    .mvt = k_mvt_topas, .decode = NULL,
};

static const WmbusMVT k_mvt_calec[] = {
    {"AMT", 0xFF, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_aquametro_calec = {
    .id = "calec", .title = "Aquametro CALEC heat",
    .mvt = k_mvt_calec, .decode = NULL,
};
