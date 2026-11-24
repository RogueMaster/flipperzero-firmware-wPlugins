#pragma once
#include <furi.h>
#include <stdint.h>
#define GTA_LOADING_PATH        APP_ASSETS_PATH("plugins/gta_loading.fal")
#define GTA_LOADING_ID          "gta_loading"
#define GTA_LOADING_API_VERSION 2u
typedef struct {
    uint32_t size;
    void* (*begin)(void);
    void (*end)(void* context);
} GtaLoadingApi;

typedef struct GtaLoadingHandle GtaLoadingHandle;
/* Local, synchronous module transition; no deferred app launch or global record. */
GtaLoadingHandle* gta_loading_show(void);
void gta_loading_hide(GtaLoadingHandle* handle);
