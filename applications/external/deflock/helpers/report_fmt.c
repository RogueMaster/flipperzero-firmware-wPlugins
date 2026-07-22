// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "report_fmt.h"

#include <math.h>
#include <stdio.h>

void fmt_mac(char* out, size_t out_len, const uint8_t mac[6]) {
    snprintf(
        out,
        out_len,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

void fmt_coord(char* out, size_t out_len, float value, const char* fallback) {
    if(isnan(value)) {
        snprintf(out, out_len, "%s", fallback);
    } else {
        snprintf(out, out_len, "%.6f", (double)value);
    }
}
