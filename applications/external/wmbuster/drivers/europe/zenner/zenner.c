/* Zenner family — Minomess water, C5-ISF heat/cool, CalToSe HCA,
 * legacy ZRM/ZEN-coded meters.
 * wmbusmeters: drivers/src/{minomess,c5isf,caltose,zennerhca}.xmq. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt_minomess[] = {
    {"ZRI", 0x00, 0x07},
    {"ZRI", 0x01, 0x06},
    {"ZRI", 0x01, 0x16},
    {"ZRI", 0x16, 0x00},
    {"ZRI", 0xFF, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_zenner_minomess = {
    .id = "minomess", .title = "Zenner Minomess water",
    .mvt = k_mvt_minomess, .decode = NULL,
};

static const WmbusMVT k_mvt_c5isf[] = {
    {"ZRI", 0x88, 0x04},
    {"ZRI", 0x88, 0x07},
    {"ZRI", 0x88, 0x0D},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_zenner_c5isf = {
    .id = "c5isf", .title = "Zenner C5-ISF heat",
    .mvt = k_mvt_c5isf, .decode = NULL,
};

/* CalToSe — Zenner HCA (and the legacy ZRM/ZEN codes). */
static const WmbusMVT k_mvt_zenhca[] = {
    {"ZRI", 0xFC, 0x08},
    {"ZRI", 0x16, 0x0B},   /* zenner0b — Zenner cooling-out 0x0b */
    {"ZRM", 0xFF, 0x08},
    {"ZEN", 0xFF, 0x07},
    {"ZRM", 0xFF, 0x07},
    {"ZRM", 0xFF, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_zenner_hca = {
    .id = "zenner-hca", .title = "Zenner HCA",
    .mvt = k_mvt_zenhca, .decode = NULL,
};
