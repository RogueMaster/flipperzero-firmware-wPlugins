/* Sensus — iPERL water and PolluCom F heat.
 * wmbusmeters references: drivers/src/{iperl,pollucomf}.xmq. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt_iperl[] = {
    {"SEN", 0x68, 0x06},
    {"SEN", 0x68, 0x07},
    {"SEN", 0x7C, 0x07},
    {"SEN", 0xFF, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_sensus_iperl = {
    .id = "sensus-iperl", .title = "Sensus iPERL water",
    .mvt = k_mvt_iperl, .decode = NULL,
};

static const WmbusMVT k_mvt_pollucom[] = {
    {"SEN", 0x1D, 0x04},
    {"SEN", 0xFF, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_sensus_pollucom = {
    .id = "sensus-pollucom", .title = "Sensus PolluCom F heat",
    .mvt = k_mvt_pollucom, .decode = NULL,
};

/* Spanner-Pollux water (legacy Sensus) */
static const WmbusMVT k_mvt_spx[] = {
    {"SPX", 0xFF, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_sensus_spx = {
    .id = "sensus-spx", .title = "Sensus/Spanner water",
    .mvt = k_mvt_spx, .decode = NULL,
};
