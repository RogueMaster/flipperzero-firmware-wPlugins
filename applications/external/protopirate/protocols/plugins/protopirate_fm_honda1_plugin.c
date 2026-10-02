#include "../protopirate_protocol_plugins.h"
#include "../protocols_common.h"
#include "../honda_static.h"

static const SubGhzProtocol* const protopirate_protocol_registry_fm_honda1_items[] = {
    &honda_static_protocol,
};

static const SubGhzProtocolRegistry protopirate_protocol_registry_fm_honda1 = {
    .items = protopirate_protocol_registry_fm_honda1_items,
    .size = sizeof(protopirate_protocol_registry_fm_honda1_items) /
            sizeof(protopirate_protocol_registry_fm_honda1_items[0]),
};

static const ProtoPirateProtocolPlugin protopirate_fm_honda1_plugin = {
    .plugin_name = "ProtoPirate FM Honda1 Registry",
    .kind = ProtoPirateProtocolPluginKindRx,
    .route = ProtoPirateProtocolRegistryRouteFMHonda1,
    .registry = &protopirate_protocol_registry_fm_honda1,
    .release = NULL,
};

static const FlipperAppPluginDescriptor protopirate_fm_honda1_plugin_descriptor = {
    .appid = PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION,
    .entry_point = &protopirate_fm_honda1_plugin,
};

const FlipperAppPluginDescriptor* protopirate_fm_honda1_plugin_ep(void) {
    return &protopirate_fm_honda1_plugin_descriptor;
}
