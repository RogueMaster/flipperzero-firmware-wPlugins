#include "fn_ident.h"

#include <string.h>

#define FN_FFD_3AH_LEN 2u
#define FN_FW_33H_LEN 17u
#define FN_FW_33H_VERSION_LEN 16u

static FnFfd fn_ffd_from_raw(uint8_t raw) {
    switch(raw) {
    case FnFfdNone:
    case FnFfd1_05:
    case FnFfd1_1:
    case FnFfd1_2:
        return (FnFfd)raw;
    default:
        return FnFfdUnknown;
    }
}

bool fn_parse_ffd_3ah(const uint8_t* data, size_t len, FnFfdInfo* out) {
    if(!data || !out || len != FN_FFD_3AH_LEN) return false;

    FnFfdInfo info;
    info.current_raw = data[0];
    info.current = fn_ffd_from_raw(data[0]);
    info.max_raw = data[1];
    info.max = fn_ffd_from_raw(data[1]);

    *out = info;
    return true;
}

bool fn_parse_fw_33h(const uint8_t* data, size_t len, FnFwInfo* out) {
    if(!data || !out || len != FN_FW_33H_LEN) return false;

    FnFwInfo info;
    memset(&info, 0, sizeof(info));
    for(size_t i = 0; i < FN_FW_33H_VERSION_LEN; ++i) {
        info.version[i] = (char)data[i];
    }
    info.version[FN_FW_33H_VERSION_LEN] = '\0';
    info.mode_raw = data[FN_FW_33H_VERSION_LEN];
    /* Эвристика проекта: mode_raw == 0 — опытный образец (МГМ), 1 — боевой. */
    info.is_mgm = (info.mode_raw == 0);

    *out = info;
    return true;
}
