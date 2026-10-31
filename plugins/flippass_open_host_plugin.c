#include "flippass_open_host_plugin.h"
#include "flippass_open_host_room.h"
#include "flippass_crypto_plugin.h"

/* Zero padding inside this plugin's own .text and .rodata sections: the loader maps
 * each section as one block, so the padding enlarges the two blocks that survive the
 * open intact and are freed for the editor right after it. */
static const uint8_t flippass_open_host_text_room[FLIPPASS_OPEN_HOST_TEXT_ROOM_BYTES]
    __attribute__((used, aligned(4), section(".text.flippass_successor_room"))) = {0};
static const uint8_t flippass_open_host_rodata_room[FLIPPASS_OPEN_HOST_RODATA_ROOM_BYTES]
    __attribute__((used, aligned(4), section(".rodata.flippass_successor_room"))) = {0};

static const FlipPassOpenHostPluginV2 flippass_open_host_plugin = {
    .api_version = FLIPPASS_OPEN_HOST_PLUGIN_API_VERSION,
    .execute = flippass_open_host_execute,
    .get_crypto_api = flippass_crypto_plugin_api,
    .successor_text_room = flippass_open_host_text_room,
    .successor_rodata_room = flippass_open_host_rodata_room,
};

static const FlipperAppPluginDescriptor flippass_open_host_descriptor = {
    .appid = FLIPPASS_OPEN_HOST_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OPEN_HOST_PLUGIN_API_VERSION,
    .entry_point = &flippass_open_host_plugin,
};

const FlipperAppPluginDescriptor* flippass_open_host_plugin_ep(void) {
    return &flippass_open_host_descriptor;
}
