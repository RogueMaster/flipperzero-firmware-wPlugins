// SPDX-License-Identifier: GPL-3.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    PcKeyboard,
    PcDiskRw,
    PcDiskRo,
    PcModeCount
} PcMode;
typedef enum {
    PcNoResponse,
    PcHidConfigured,
    PcHidSent,
    PcDiskRead,
    PcDiskWritten,
    PcIoError,
    PcSignalCount
} PcSignal;
PcSignal pc_observation(
    PcMode mode,
    bool configured,
    bool sent,
    uint32_t reads,
    uint32_t writes,
    uint32_t errors);
