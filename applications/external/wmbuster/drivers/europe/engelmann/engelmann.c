/* Engelmann — Sensostar heat, Waterstar water, HCA-E2.
 * wmbusmeters: drivers/src/{sensostar,engelmann_faw,hcae2,
 *                            waterstarm}.xmq. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt_sensostar[] = {
    {"EFE", 0x00, 0x04},
    {"EFE", 0xFF, 0x04},
    {"EFN", 0xFF, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_engelmann_sensostar = {
    .id = "sensostar", .title = "Engelmann Sensostar",
    .mvt = k_mvt_sensostar, .decode = NULL,
};

static const WmbusMVT k_mvt_faw[] = {
    {"EFE", 0x00, 0x07},
    {"EFE", 0xFF, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_engelmann_faw = {
    .id = "engelmann-faw", .title = "Engelmann Waterstar",
    .mvt = k_mvt_faw, .decode = NULL,
};

static const WmbusMVT k_mvt_hcae2[] = {
    {"EFE", 0x31, 0x08},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_engelmann_hcae2 = {
    .id = "hcae2", .title = "Engelmann HCA-E2",
    .mvt = k_mvt_hcae2, .decode = NULL,
};

/* DWZ Waterstar M (rebadged Engelmann firmware variant). */
static const WmbusMVT k_mvt_waterstarm[] = {
    {"DWZ", 0x00, 0x06},
    {"DWZ", 0x00, 0x07},
    {"DWZ", 0x02, 0x06},
    {"DWZ", 0x02, 0x07},
    {"EFE", 0x03, 0x07},
    {"EFE", 0x70, 0x07},
    {"DWZ", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_waterstarm = {
    .id = "waterstarm", .title = "DWZ Waterstar M",
    .mvt = k_mvt_waterstarm, .decode = NULL,
};
