#pragma once

typedef struct {
    const char* appid;
    unsigned int ep_api_version;
    const void* entry_point;
} FlipperAppPluginDescriptor;
