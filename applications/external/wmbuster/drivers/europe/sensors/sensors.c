/* Environmental + auxiliary sensors. None of these meters carry
 * heat/water/gas readings — they show up on the same wM-Bus
 * radio and are useful enough to label.
 *
 * wmbusmeters: drivers/src/{lansendw,lansenpu,lansenrp,lansensm,
 *   lansenth,munia,piigth,elvsense,ei6500}.xmq. */

#include "../../engine/driver.h"

/* Lansen sensors — door-window, pulse, room-temp, smoke. */
static const WmbusMVT k_mvt_lansendw[] = {
    {"LAS", 0x07, 0x1D},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_lansendw = {
    .id = "lansendw", .title = "Lansen door/window",
    .mvt = k_mvt_lansendw, .decode = NULL,
};

static const WmbusMVT k_mvt_lansenth[] = {
    {"LAS", 0x07, 0x1B},
    {"LAS", 0x09, 0x1B},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_lansenth = {
    .id = "lansenth", .title = "Lansen TH sensor",
    .mvt = k_mvt_lansenth, .decode = NULL,
};

static const WmbusMVT k_mvt_lansenpu[] = {
    {"LAS", 0x14, 0x00},
    {"LAS", 0x1B, 0x00},
    {"LAS", 0x0B, 0x02},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_lansenpu = {
    .id = "lansenpu", .title = "Lansen pulse",
    .mvt = k_mvt_lansenpu, .decode = NULL,
};

static const WmbusMVT k_mvt_lansenrp[] = {
    {"LAS", 0x0B, 0x32},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_lansenrp = {
    .id = "lansenrp", .title = "Lansen RP",
    .mvt = k_mvt_lansenrp, .decode = NULL,
};

static const WmbusMVT k_mvt_lansensm[] = {
    {"LAS", 0x03, 0x1A},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_lansensm = {
    .id = "lansensm", .title = "Lansen smoke",
    .mvt = k_mvt_lansensm, .decode = NULL,
};

/* Weptech Munia temperature/humidity */
static const WmbusMVT k_mvt_munia[] = {
    {"WEP", 0x02, 0x1B},
    {"WEP", 0x04, 0x1B},
    {"WEP", 0xFF, 0x1B},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_munia = {
    .id = "munia", .title = "Weptech Munia TH",
    .mvt = k_mvt_munia, .decode = NULL,
};

/* PII GTH sensor */
static const WmbusMVT k_mvt_piigth[] = {
    {"PII", 0x01, 0x1B},
    {"PII", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_piigth = {
    .id = "piigth", .title = "PII GTH sensor",
    .mvt = k_mvt_piigth, .decode = NULL,
};

/* ELV / eQ-3 sensors */
static const WmbusMVT k_mvt_elvsense[] = {
    {"ELV", 0x20, 0x1B},
    {"ELV", 0x50, 0x1B},
    {"ELV", 0x51, 0x1B},
    {"ELV", 0x52, 0x1B},
    {"ELV", 0x53, 0x1B},
    {"ELV", 0x54, 0x1B},
    {"ELV", 0xFF, 0xFF},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_elvsense = {
    .id = "elvsense", .title = "ELV/eQ-3 sensor",
    .mvt = k_mvt_elvsense, .decode = NULL,
};

/* EI 6500 smoke detector */
static const WmbusMVT k_mvt_ei6500[] = {
    {"EIE", 0x0C, 0x1A},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_ei6500 = {
    .id = "ei6500", .title = "EI6500 smoke",
    .mvt = k_mvt_ei6500, .decode = NULL,
};
