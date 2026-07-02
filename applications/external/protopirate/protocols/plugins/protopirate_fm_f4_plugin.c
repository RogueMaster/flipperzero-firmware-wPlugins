#include "../protopirate_protocol_plugins.h"
#include "../protocols_common.h"
#include "../ford_v1.h"
#include "../ford_v2.h"
#include "../ford_v3.h"
#include "../honda_v2.h"

static const SubGhzProtocol* const protopirate_protocol_registry_fm_f4_items[] = {
    &ford_protocol_v1,
    &ford_protocol_v2,
    &ford_protocol_v3,
    &honda_v2_protocol,
};

static const SubGhzProtocolRegistry protopirate_protocol_registry_fm_f4 = {
    .items = protopirate_protocol_registry_fm_f4_items,
    .size = sizeof(protopirate_protocol_registry_fm_f4_items) /
            sizeof(protopirate_protocol_registry_fm_f4_items[0]),
};

static const ProtoPirateProtocolPlugin protopirate_fm_f4_plugin = {
    .plugin_name = "ProtoPirate FM F4 Registry",
    .kind = ProtoPirateProtocolPluginKindRx,
    .route = ProtoPirateProtocolRegistryRouteFMF4,
    .registry = &protopirate_protocol_registry_fm_f4,
    .release = NULL,
};

static const FlipperAppPluginDescriptor protopirate_fm_f4_plugin_descriptor = {
    .appid = PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION,
    .entry_point = &protopirate_fm_f4_plugin,
};

const FlipperAppPluginDescriptor* protopirate_fm_f4_plugin_ep(void) {
    return &protopirate_fm_f4_plugin_descriptor;
}
