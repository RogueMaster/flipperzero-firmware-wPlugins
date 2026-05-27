/* Long-tail vendors that ship a single product family. Each has
 * a wmbusmeters declarative driver but doesn't justify its own
 * folder in this project. They all ride on the OMS walker.
 */

#include "../../engine/driver.h"

/* Aventies (AAA) — water + HCA */
static const WmbusMVT k_mvt_aventies[] = {
    {"AAA", 0x25, 0x07},
    {"AAA", 0x55, 0x08},
    {"AAA", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_aventies = {
    .id = "aventies", .title = "Aventies meter",
    .mvt = k_mvt_aventies, .decode = NULL,
};

/* Amrest Unismart gas */
static const WmbusMVT k_mvt_unismart[] = {
    {"AMX", 0x01, 0x03},
    {"AMX", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_unismart = {
    .id = "unismart", .title = "Amrest Unismart gas",
    .mvt = k_mvt_unismart, .decode = NULL,
};

/* Innotas / Rossweiner Eurisii HCA */
static const WmbusMVT k_mvt_eurisii[] = {
    {"INE", 0x55, 0x08},
    {"RAM", 0x55, 0x08},
    {"INE", 0xFF, 0xFF},
    {"RAM", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_eurisii = {
    .id = "eurisii", .title = "Innotas Eurisii HCA",
    .mvt = k_mvt_eurisii, .decode = NULL,
};

/* GWF gas + Enercal heat */
static const WmbusMVT k_mvt_gwf[] = {
    {"GWF", 0x08, 0x04},
    {"GWF", 0x3C, 0x03},
    {"GWF", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_gwf = {
    .id = "gwf", .title = "GWF meter",
    .mvt = k_mvt_gwf, .decode = NULL,
};

/* ELR (Emerlin868, EV200) */
static const WmbusMVT k_mvt_elr[] = {
    {"ELR", 0x0D, 0x07},
    {"ELR", 0x11, 0x37},
    {"ELR", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_elr = {
    .id = "elr", .title = "ELR meter",
    .mvt = k_mvt_elr, .decode = NULL,
};

/* Werhle MOD-WM */
static const WmbusMVT k_mvt_werhle[] = {
    {"WZG", 0x03, 0x16},
    {"WZG", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_werhle = {
    .id = "werhle", .title = "Werhle MOD-WM",
    .mvt = k_mvt_werhle, .decode = NULL,
};

/* Watertech */
static const WmbusMVT k_mvt_wtt[] = {
    {"WTT", 0x59, 0x07},
    {"WTT", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_watertech = {
    .id = "watertech", .title = "Watertech water",
    .mvt = k_mvt_wtt, .decode = NULL,
};

/* WEH 07 water */
static const WmbusMVT k_mvt_weh[] = {
    {"WEH", 0x03, 0x07},
    {"WEH", 0xFE, 0x07},
    {"WEH", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_weh = {
    .id = "weh07", .title = "WEH 07 water",
    .mvt = k_mvt_weh, .decode = NULL,
};

/* Q400 / Qualcosonic (UAB Axis) */
static const WmbusMVT k_mvt_axis[] = {
    {"AXI", 0x01, 0x07},
    {"AXI", 0x10, 0x07},
    {"AXI", 0x0B, 0x0D},
    {"AXI", 0x0C, 0x0D},
    {"AXI", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_axis = {
    .id = "axis-q400", .title = "UAB Axis water/heat",
    .mvt = k_mvt_axis, .decode = NULL,
};

/* VIPA Kaden HCA */
static const WmbusMVT k_mvt_vip[] = {
    {"VIP", 0x1E, 0x08},
    {"VIP", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_vipa_kaden = {
    .id = "kaden", .title = "VIPA Kaden HCA",
    .mvt = k_mvt_vip, .decode = NULL,
};

/* Relay PadPuls / generic */
static const WmbusMVT k_mvt_rel[] = {
    {"REL", 0x41, 0x00},
    {"REL", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_relay = {
    .id = "relay", .title = "Relay gateway",
    .mvt = k_mvt_rel, .decode = NULL,
};

/* Ecomess Picoflux water */
static const WmbusMVT k_mvt_ecm[] = {
    {"ECM", 0x05, 0x07},
    {"ECM", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_picoflux = {
    .id = "picoflux", .title = "Ecomess Picoflux",
    .mvt = k_mvt_ecm, .decode = NULL,
};

/* Fiotech */
static const WmbusMVT k_mvt_fio[] = {
    {"FIO", 0x01, 0x07},
    {"FIO", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_fiotech = {
    .id = "fiotech", .title = "Fiotech water",
    .mvt = k_mvt_fio, .decode = NULL,
};

/* Sappel water (legacy Diehl rebadge) */
static const WmbusMVT k_mvt_sap[] = {
    {"SAP", 0xFF, 0x07},
    {"SAP", 0x00, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_sappel = {
    .id = "sappel", .title = "Sappel water",
    .mvt = k_mvt_sap, .decode = NULL,
};

/* Hydrometer fallback */
static const WmbusMVT k_mvt_hyd[] = {
    {"HYD", 0xFF, 0x04},
    {"HYD", 0xFF, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_hydrometer = {
    .id = "hydrometer", .title = "Hydrometer meter",
    .mvt = k_mvt_hyd, .decode = NULL,
};

/* BFW 240-Radio: full decoder lives in misc/bfw240radio.c — this file
 * intentionally no longer registers a stub for it. */
