#pragma once

#include "sr_types.h"
#include "sr_source_codec.h"

/*
 * The device-side Source facade. This header therefore needs no furi includes of its own.
 *
 * IO operations (probe / start_wardrive / stop) go through SrIo*;
 * command building, probe decisions, and line parsing go through the codec (★ pure logic).
 * The rationale for the split is in ADR-008.
 */

typedef struct SrIo SrIo;

typedef struct {
    const SrSourceCodec* codec;
    bool (*probe)(SrIo* io, SrFirmwareInfo* out);
    bool (*start_wardrive)(SrIo* io, const SrScanCfg* cfg);
    bool (*stop)(SrIo* io);
} SigRoamSource;
