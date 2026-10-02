#include "../protopirate_protocol_plugins.h"
#include "../protocols_common.h"
#include "../vag.h"

static const SubGhzProtocol* const protopirate_protocol_registry_am_vag_items[] = {
    &vag_protocol,
};

static const SubGhzProtocolRegistry protopirate_protocol_registry_am_vag = {
    .items = protopirate_protocol_registry_am_vag_items,
    .size = sizeof(protopirate_protocol_registry_am_vag_items) /
            sizeof(protopirate_protocol_registry_am_vag_items[0]),
};

static const ProtoPirateProtocolPlugin protopirate_am_vag_plugin = {
    .plugin_name = "ProtoPirate AM VAG Registry",
    .kind = ProtoPirateProtocolPluginKindRx,
    .route = ProtoPirateProtocolRegistryRouteAMVag,
    .registry = &protopirate_protocol_registry_am_vag,
    .release = NULL,
};

static const FlipperAppPluginDescriptor protopirate_am_vag_plugin_descriptor = {
    .appid = PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION,
    .entry_point = &protopirate_am_vag_plugin,
};

const FlipperAppPluginDescriptor* protopirate_am_vag_plugin_ep(void) {
    return &protopirate_am_vag_plugin_descriptor;
}
