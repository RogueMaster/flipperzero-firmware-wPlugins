#include "../protopirate_protocol_plugins.h"
#include "../protocols_common.h"

#ifndef PP_TX_PROTOCOL_HEADER
#error "PP_TX_PROTOCOL_HEADER must be defined for TX protocol plugins"
#endif

#include PP_TX_PROTOCOL_HEADER

#ifndef PP_TX_PROTOCOL_NAME
#error "PP_TX_PROTOCOL_NAME must be defined for TX protocol plugins"
#endif

#ifndef PP_TX_PROTOCOL_ITEM
#error "PP_TX_PROTOCOL_ITEM must be defined for TX protocol plugins"
#endif

static const SubGhzProtocol* const protopirate_tx_protocol_registry_items[] = {
    &PP_TX_PROTOCOL_ITEM,
};

static const SubGhzProtocolRegistry protopirate_tx_protocol_registry = {
    .items = protopirate_tx_protocol_registry_items,
    .size = 1U,
};

static const ProtoPirateProtocolPlugin protopirate_tx_protocol_plugin = {
    .plugin_name = PP_TX_PROTOCOL_NAME,
    .kind = ProtoPirateProtocolPluginKindTx,
    .route = ProtoPirateProtocolRegistryRouteAMDefault,
    .protocol_name = PP_TX_PROTOCOL_NAME,
    .registry = &protopirate_tx_protocol_registry,
    .release = pp_shared_upload_release,
};

static const FlipperAppPluginDescriptor protopirate_tx_protocol_plugin_descriptor = {
    .appid = PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION,
    .entry_point = &protopirate_tx_protocol_plugin,
};

const FlipperAppPluginDescriptor* protopirate_tx_protocol_plugin_ep(void) {
    return &protopirate_tx_protocol_plugin_descriptor;
}
