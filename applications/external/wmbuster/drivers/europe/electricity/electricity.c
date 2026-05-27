/* Smart electricity meters from many vendors. They almost all
 * speak OMS once decrypted, so the generic walker handles them
 * — these descriptors exist primarily to attach a friendly title
 * to the meter list and detail view.
 *
 * wmbusmeters: drivers/src/{abbb23,ebzwmbe,ehzp,em24,esyswm,
 *   gransystems,iem3000,ime,nemo,nzr,eltako}.xmq. */

#include "../../engine/driver.h"

/* ABB B23/B24 */
static const WmbusMVT k_mvt_abb[] = {
    {"ABB", 0x20, 0x02},
    {"ABB", 0xFF, 0x02},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_abb_b23 = {
    .id = "abbb23", .title = "ABB B23 elec",
    .mvt = k_mvt_abb, .decode = NULL,
};

/* EBZ wMBus electric. */
static const WmbusMVT k_mvt_ebz[] = {
    {"EBZ", 0x01, 0x02},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_ebzwmbe = {
    .id = "ebzwmbe", .title = "EBZ wMBus elec",
    .mvt = k_mvt_ebz, .decode = NULL,
};

/* EMH eHZP / generic NZR-EMH */
static const WmbusMVT k_mvt_emh[] = {
    {"EMH", 0x00, 0x02},
    {"EMH", 0x02, 0x02},
    {"EMH", 0xFF, 0x02},
    {"NZR", 0x00, 0x02},
    {"NZR", 0xFF, 0x02},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_emh_ehzp = {
    .id = "ehzp", .title = "EMH eHZ elec",
    .mvt = k_mvt_emh, .decode = NULL,
};

/* EasyMeter ESYS-WM */
static const WmbusMVT k_mvt_esys[] = {
    {"ESY", 0x11, 0x02},
    {"ESY", 0x30, 0x37},
    {"ESY", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_esys_wm = {
    .id = "esyswm", .title = "EasyMeter elec",
    .mvt = k_mvt_esys, .decode = NULL,
};

/* Carlo Gavazzi EM24 */
static const WmbusMVT k_mvt_em24[] = {
    {"GAV", 0x00, 0x02},
    {"GAV", 0xFF, 0x02},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_em24 = {
    .id = "em24", .title = "Carlo Gavazzi EM24",
    .mvt = k_mvt_em24, .decode = NULL,
};

/* Schneider iEM3000 */
static const WmbusMVT k_mvt_iem3000[] = {
    {"SEC", 0x13, 0x02},
    {"SEC", 0x15, 0x02},
    {"SEC", 0x18, 0x02},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_iem3000 = {
    .id = "iem3000", .title = "Schneider iEM3000",
    .mvt = k_mvt_iem3000, .decode = NULL,
};

/* IME Nemo + IME generic */
static const WmbusMVT k_mvt_ime[] = {
    {"IME", 0x1D, 0x02},
    {"IME", 0x66, 0x02},
    {"IME", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_ime = {
    .id = "ime-nemo", .title = "IME elec",
    .mvt = k_mvt_ime, .decode = NULL,
};

/* Eltako, Gran-Systems, generic */
static const WmbusMVT k_mvt_eltako[] = {
    {"ELT", 0x01, 0x02},
    {"ELT", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_eltako = {
    .id = "eltako", .title = "Eltako elec",
    .mvt = k_mvt_eltako, .decode = NULL,
};

static const WmbusMVT k_mvt_gss[] = {
    {"GSS", 0x01, 0x02},
    {"GSS", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_gransystems = {
    .id = "gransystems", .title = "Gran-Systems elec",
    .mvt = k_mvt_gss, .decode = NULL,
};
