/* Kamstrup A/S — MULTICAL heat, flowIQ water, OmniPower elec.
 *
 * All Kamstrup meters in scope here speak OMS DIF/VIF once the
 * link header is stripped, so the generic walker handles them.
 * The MULTICAL 21 short telegram (CI=0x78/0x79) is the one
 * exception — its compact frame wraps two pre-defined fields and
 * needs a dedicated decoder. That port is left as a TODO with a
 * clear pointer to the wmbusmeters reference.
 *
 * Sources:
 *   _refs/wmbusmeters/drivers/src/{kamheat,kamwater,kampress,
 *                                   omnipower}.xmq
 */

#include "../../engine/driver.h"

/* MULTICAL — every heat-meter version in the wmbusmeters list. */
static const WmbusMVT k_mvt_multical[] = {
    {"KAM", 0x19, 0x04},
    {"KAM", 0x1C, 0x04},
    {"KAM", 0x30, 0x04},
    {"KAM", 0x30, 0x0C},
    {"KAM", 0x30, 0x0D},
    {"KAM", 0x34, 0x04},
    {"KAM", 0x34, 0x0A},
    {"KAM", 0x34, 0x0B},
    {"KAM", 0x34, 0x0C},
    {"KAM", 0x34, 0x0D},
    {"KAM", 0x35, 0x04},
    {"KAM", 0x35, 0x0C},
    {"KAM", 0x39, 0x04},
    {"KAM", 0x40, 0x04},
    {"KAM", 0x40, 0x0C},
    /* Wildcard medium fallbacks for older firmware variants. */
    {"KAM", 0xFF, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_kamstrup_multical = {
    .id = "kam-multical", .title = "Kamstrup MULTICAL",
    .mvt = k_mvt_multical, .decode = NULL,
};

/* flowIQ — water (incl. cold/warm). */
static const WmbusMVT k_mvt_flowiq[] = {
    {"KAM", 0x1B, 0x06},
    {"KAM", 0x1B, 0x16},
    {"KAM", 0x1D, 0x16},
    {"KAW", 0x3A, 0x16},
    {"KAW", 0x3C, 0x16},
    {"KAM", 0xFF, 0x07},
    {"KAM", 0xFF, 0x16},
    {"KAM", 0xFF, 0x06},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_kamstrup_flowiq = {
    .id = "kam-flowiq", .title = "Kamstrup flowIQ water",
    .mvt = k_mvt_flowiq, .decode = NULL,
};

/* OmniPower (electricity) and Carlo-Gavazzi-rebadge KAM 0x33,02. */
static const WmbusMVT k_mvt_omnipower[] = {
    {"KAM", 0x30, 0x02},
    {"KAM", 0x33, 0x02},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_kamstrup_omnipower = {
    .id = "kam-omnipower", .title = "Kamstrup OmniPower",
    .mvt = k_mvt_omnipower, .decode = NULL,
};

/* Kampress — pressure sensor. */
static const WmbusMVT k_mvt_kampress[] = {
    {"KAM", 0x01, 0x18},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_kamstrup_kampress = {
    .id = "kam-press", .title = "Kamstrup pressure",
    .mvt = k_mvt_kampress, .decode = NULL,
};
