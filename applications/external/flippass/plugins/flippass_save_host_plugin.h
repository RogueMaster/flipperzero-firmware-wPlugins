#pragma once

#include "../flippass.h"

#define FLIPPASS_SAVE_HOST_PLUGIN_APP_ID      "flippass_save_host"
#define FLIPPASS_SAVE_HOST_PLUGIN_API_VERSION 1u

/* Synchronous phase owner. No callbacks or pointers into this module may survive
 * a call. The root proxy pins it across nested saves and unloads on the last return. */
typedef struct {
    uint32_t api_version;
    bool (*current_database)(App*, const char*, const char*, FuriString*);
    bool (*execute)(
        App*,
        const char*,
        const char*,
        FlipPassKdbxCipher,
        uint32_t,
        uint64_t,
        FuriString*);
    bool (*debug_after_open)(App*, FuriString*);
} FlipPassSaveHostPluginV1;

bool flippass_save_host_current_database(App*, const char*, const char*, FuriString*);
bool flippass_save_host_execute(
    App*,
    const char*,
    const char*,
    FlipPassKdbxCipher,
    uint32_t,
    uint64_t,
    FuriString*);
#if FLIPPASS_ENABLE_DEBUG_SAVE_HOOK
bool flippass_save_host_debug_after_open(App*, FuriString*);
#endif
