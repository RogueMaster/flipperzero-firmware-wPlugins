#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    GtaDispatchAll,
    GtaDispatchViceCity,
    GtaDispatchKeys,
    GtaDispatchGrassrivers,
    GtaDispatchPortGellhorn,
    GtaDispatchAmbrosia,
    GtaDispatchMountKalaga,
    GtaDispatchOther,
    GtaDispatchRegionCount,
} GtaDispatchRegion;

enum {
    GtaDispatchSourceMoses,
    GtaDispatchSourceRandy,
    GtaDispatchFirstAsset,
};

typedef struct {
    uint16_t id;
    uint8_t source;
    uint8_t regions;
} GtaDispatchTarget;

extern const char* const gta_dispatch_regions[GtaDispatchRegionCount];
extern const char* const gta_dispatch_assets[];
extern const GtaDispatchTarget gta_dispatch_targets[];
extern const size_t gta_dispatch_target_count;
