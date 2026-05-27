/* Elster (Honeywell) — gas, F-series heat, water meters.
 * wmbusmeters: drivers/src/elster.xmq + family wildcards. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt[] = {
    {"ELS", 0x81, 0x03},
    {"ELS", 0xFF, 0x03},
    {"ELS", 0xFF, 0x04},
    {"ELS", 0xFF, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_elster = {
    .id = "elster", .title = "Elster meter",
    .mvt = k_mvt, .decode = NULL,
};
