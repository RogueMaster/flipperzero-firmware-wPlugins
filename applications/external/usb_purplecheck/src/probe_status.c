// SPDX-License-Identifier: GPL-3.0
#include "probe_status.h"
PcSignal pc_observation(
    PcMode mode,
    bool configured,
    bool sent,
    uint32_t reads,
    uint32_t writes,
    uint32_t errors) {
    if(errors || (mode == PcDiskRo && writes)) return PcIoError;
    if(mode == PcKeyboard) return sent ? PcHidSent : configured ? PcHidConfigured : PcNoResponse;
    if(mode == PcDiskRw || mode == PcDiskRo)
        return writes ? PcDiskWritten : reads ? PcDiskRead : PcNoResponse;
    return PcNoResponse;
}
