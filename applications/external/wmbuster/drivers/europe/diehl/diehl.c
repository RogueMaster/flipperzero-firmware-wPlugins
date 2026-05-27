/* Diehl Metering family — Hydrus, Sharky, IZAR, Aerius gas, etc.
 *
 * All drivers in this file ride on the generic OMS DIF/VIF walker.
 * The IZAR water meter ships PRIOS-encrypted payloads that the
 * walker cannot decode — we register a stub driver that produces
 * a labelled hex dump until somebody ports
 * `_refs/wmbusmeters/src/driver_izar.cc` (322 lines).
 *
 * Sources of the (M,V,T) triples below:
 *   _refs/wmbusmeters/drivers/src/{hydrus,sharky,sharky774,
 *                                   aerius,dme173,dme_07}.xmq
 */

#include "../../engine/driver.h"

/* ---- Diehl Hydrus (water) ---- */
static const WmbusMVT k_mvt_hydrus[] = {
    {"DME", 0x70, 0x06},
    {"DME", 0x70, 0x07},
    {"DME", 0x70, 0x16},
    {"DME", 0x76, 0x07},
    {"HYD", 0x24, 0x07},
    {"HYD", 0x8B, 0x06},
    {"HYD", 0x8B, 0x07},
    /* Note: (HYD,0x07,0x85) and (HYD,0x07,0x86) used to live here but
     * upstream wmbusmeters claims them for IZAR/PRIOS — see izar.c. */
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_diehl_hydrus = {
    .id = "diehl-hydrus", .title = "Diehl Hydrus water",
    .mvt = k_mvt_hydrus, .decode = NULL,
};

/* ---- Diehl Sharky (heat / cooling) ---- */
static const WmbusMVT k_mvt_sharky[] = {
    {"DME", 0x40, 0x04},
    {"DME", 0x41, 0x04},
    {"DME", 0x41, 0x0C},
    {"DME", 0x41, 0x0D},
    {"HYD", 0x20, 0x04},
    {"SAP", 0x01, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_diehl_sharky = {
    .id = "diehl-sharky", .title = "Diehl Sharky heat",
    .mvt = k_mvt_sharky, .decode = NULL,
};

/* ---- Diehl Aerius gas ---- */
static const WmbusMVT k_mvt_aerius[] = {
    {"DME", 0x30, 0x03},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_diehl_aerius = {
    .id = "diehl-aerius", .title = "Diehl Aerius gas",
    .mvt = k_mvt_aerius, .decode = NULL,
};

/* ---- Generic Diehl water (DME 173, dme_07) ---- */
static const WmbusMVT k_mvt_diehl_water[] = {
    {"DME", 0x63, 0x07},
    {"DME", 0x7B, 0x07},
    {"SAP", 0x00, 0x07},
    /* (SAP,0x15,0x07) is now in izar.c — that line of meters speaks
     * PRIOS, not raw OMS. */
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_diehl_water = {
    .id = "diehl-water", .title = "Diehl water",
    .mvt = k_mvt_diehl_water, .decode = NULL,
};

/* IZAR (Diehl/Sappel/Hydrometer water meters with PRIOS scrambling) lives
 * in its own file `izar.c` because the LFSR decoder is non-trivial — see
 * the file header comment there for the protocol port. */
