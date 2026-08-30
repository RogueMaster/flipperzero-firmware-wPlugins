#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef void (*MvtLabelCallback)(const char* name, int32_t ex, int32_t ey, void* context);

void mvt_scan_labels(
    const uint8_t* tile,
    size_t len,
    const char* const* layers,
    size_t layer_count,
    MvtLabelCallback cb,
    void* context,
    uint32_t* extent_out);
