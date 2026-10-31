#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>
#include "../flippass_crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

struct App;

#define FLIPPASS_OPEN_HOST_PLUGIN_APP_ID      "flippass_open_host"
#define FLIPPASS_OPEN_HOST_PLUGIN_API_VERSION 3u

/** The whole unlock pipeline (flippass_open.c); it calls host functions directly. */
typedef struct {
    uint32_t api_version;
    bool (*execute)(struct App* app, FuriString* error);
    const FlipPassCryptoApiV1* (*get_crypto_api)(void);
    /* Successor room: never read. It pads this plugin's .text and .rodata so the
     * blocks they free on unload can hold the editor (extras/tools/
     * flippass_plugin_room.py sizes it); referenced here so the linker keeps it. */
    const uint8_t* successor_text_room;
    const uint8_t* successor_rodata_room;
} FlipPassOpenHostPluginV2;

bool flippass_open_host_execute(struct App* app, FuriString* error);

const FlipperAppPluginDescriptor* flippass_open_host_plugin_ep(void);

#ifdef __cplusplus
}
#endif
