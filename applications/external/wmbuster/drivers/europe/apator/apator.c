/* Apator (Poland) — diverse portfolio: amiplus elec, 162/172
 * water, 08 gas, ELF heat, OP-041A water, Ultrimis water,
 * Powogaz water (APT), Aquastream (IMT).
 *
 * wmbusmeters refs: drivers/src/{amiplus,apator08,apator162,
 *   apator172,aptmbusna,op041a,elf,elf2,ultrimis,aquastream}.xmq.
 */

#include "../../engine/driver.h"

/* amiplus — smart electricity (relabelled by NES, DEV, APT). */
static const WmbusMVT k_mvt_amiplus[] = {
    {"APA", 0x01, 0x02},
    {"APA", 0x02, 0x02},
    {"APT", 0x01, 0x02},
    {"NES", 0x03, 0x02},
    {"DEV", 0x00, 0x02},
    {"DEV", 0x01, 0x02},
    {"DEV", 0x02, 0x37},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_amiplus = {
    .id = "amiplus", .title = "Apator amiplus elec",
    .mvt = k_mvt_amiplus, .decode = NULL,
};

/* Apator 162 water */
static const WmbusMVT k_mvt_a162[] = {
    {"APA", 0x05, 0x06},
    {"APA", 0x05, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_162 = {
    .id = "apator162", .title = "Apator 162 water",
    .mvt = k_mvt_a162, .decode = NULL,
};

/* Apator 172 water */
static const WmbusMVT k_mvt_a172[] = {
    {"APT", 0x04, 0x11},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_172 = {
    .id = "apator172", .title = "Apator 172 water",
    .mvt = k_mvt_a172, .decode = NULL,
};

/* Apator 08 gas */
static const WmbusMVT k_mvt_a08[] = {
    {"APT", 0x03, 0x03},
    {"APT", 0x0F, 0x0F},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_08 = {
    .id = "apator08", .title = "Apator 08 gas",
    .mvt = k_mvt_a08, .decode = NULL,
};

/* MBUS-NA1 / NA-1 water — the (APA, 0x07, 0x14) tuple now lives in
 * `apator_na1.c` which carries the real "needs key" decoder. The
 * (APA, 0x15, 0x07) tuple stays here as an OMS-walker fallback for the
 * non-encrypted variant of the meter. */
static const WmbusMVT k_mvt_na1_oms[] = {
    {"APA", 0x15, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_na1_oms = {
    .id = "apator-na1-oms", .title = "Apator NA-1 water",
    .mvt = k_mvt_na1_oms, .decode = NULL,
};

/* OP-041A water */
static const WmbusMVT k_mvt_op041a[] = {
    {"APA", 0x1A, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_op041a = {
    .id = "apator-op041a", .title = "Apator OP-041A water",
    .mvt = k_mvt_op041a, .decode = NULL,
};

/* ELF / ELF2 heat */
static const WmbusMVT k_mvt_elf[] = {
    {"APA", 0x40, 0x04},
    {"APA", 0x42, 0x04},
    {"APA", 0x42, 0x0D},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_elf = {
    .id = "apator-elf", .title = "Apator ELF heat",
    .mvt = k_mvt_elf, .decode = NULL,
};

/* Apator HCA / E.ITN (heat-cost allocator). Both byte orderings appear
 * in the wild: wmbusmeters' apatoreitn driver registers (mfr, 0x08,
 * 0x04) which is version=0x08, medium=0x04, while older firmware uses
 * the inverted (0x04, 0x08). Cover both. */
static const WmbusMVT k_mvt_apator_hca[] = {
    {"APA", 0x04, 0x08},
    {"APT", 0x04, 0x08},
    {"APA", 0x08, 0x04},
    {"APT", 0x08, 0x04},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_hca = {
    .id = "apator-hca", .title = "Apator HCA",
    .mvt = k_mvt_apator_hca, .decode = NULL,
};

/* Ultrimis water (cold) */
static const WmbusMVT k_mvt_ultrimis[] = {
    {"APA", 0x01, 0x16},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_ultrimis = {
    .id = "ultrimis", .title = "Apator Ultrimis water",
    .mvt = k_mvt_ultrimis, .decode = NULL,
};

/* Apator Powogaz / Aquastream (IMT) */
static const WmbusMVT k_mvt_aquastream[] = {
    {"IMT", 0x01, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_apator_aquastream = {
    .id = "aquastream", .title = "Apator Aquastream",
    .mvt = k_mvt_aquastream, .decode = NULL,
};
