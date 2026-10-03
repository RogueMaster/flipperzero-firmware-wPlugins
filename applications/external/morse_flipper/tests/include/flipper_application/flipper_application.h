#pragma once

#include <stdint.h>

typedef struct FlipperAppPluginDescriptor {
    const char* appid;
    uint32_t ep_api_version;
    const void* entry_point;
} FlipperAppPluginDescriptor;
