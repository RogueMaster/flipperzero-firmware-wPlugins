/* BMeters (Italy) — HYDRODIGIT, HYDROSONIS, IWM-TX5, Hydrocal M3/M4,
 * RFM-AMB temp/humidity, Hydroclima HCA. wmbusmeters: drivers/src/
 * {hydrocalm3,hydrocalm4,hydroclimav2,hydrodigit,iwmtx5,rfmamb}.xmq. */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt_hydrodigit[] = {
    {"BMT", 0x06, 0x13},
    {"BMT", 0x06, 0x17},
    {"BMT", 0x07, 0x05},
    {"BMT", 0x07, 0x13},
    {"BMT", 0x07, 0x15},
    {"BMT", 0x07, 0x17},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_bm_hydrodigit = {
    .id = "hydrodigit", .title = "BMeters HYDRODIGIT",
    .mvt = k_mvt_hydrodigit, .decode = NULL,
};

static const WmbusMVT k_mvt_iwm[] = {
    {"BMT", 0x18, 0x06},
    {"BMT", 0x18, 0x07},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_bm_iwmtx5 = {
    .id = "iwmtx5", .title = "BMeters IWM-TX5",
    .mvt = k_mvt_iwm, .decode = NULL,
};

static const WmbusMVT k_mvt_hcalm3[] = {
    {"BMT", 0x0B, 0x0D},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_bm_hydrocalm3 = {
    .id = "hydrocalm3", .title = "BMeters Hydrocal M3",
    .mvt = k_mvt_hcalm3, .decode = NULL,
};

static const WmbusMVT k_mvt_hcalm4[] = {
    {"BMT", 0x1A, 0x0D},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_bm_hydrocalm4 = {
    .id = "hydrocalm4", .title = "BMeters Hydrocal M4",
    .mvt = k_mvt_hcalm4, .decode = NULL,
};

static const WmbusMVT k_mvt_hclima[] = {
    {"BMP", 0x53, 0x08},
    {"BMP", 0x85, 0x08},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_bm_hydroclima = {
    .id = "hydroclima", .title = "BMeters Hydroclima",
    .mvt = k_mvt_hclima, .decode = NULL,
};

static const WmbusMVT k_mvt_rfmamb[] = {
    {"BMT", 0x10, 0x1B},
    { {0}, 0, 0 }
};
const WmbusDriver wmbus_drv_bm_rfmamb = {
    .id = "rfmamb", .title = "BMeters RFM-AMB",
    .mvt = k_mvt_rfmamb, .decode = NULL,
};
