#include "../protopirate_protocol_plugins.h"
#include "../scher_khan.h"
#include "../kia_v0.h"
#include "../kia_v2.h"
#include "../kia_v3_v4.h"
#include "../kia_v5.h"
#include "../kia_v6.h"
#include "../kia_v7.h"
#include "../ford_v1.h"
#include "../ford_v2.h"
#include "../honda_static.h"
#include "../land_rover_v0.h"
#include "../mazda_v0.h"
#include "../psa.h"

static const SubGhzProtocol* const protopirate_protocol_registry_fm_items[] = {
    &subghz_protocol_scher_khan,
    &kia_protocol_v0,
    &kia_protocol_v2,
    &kia_protocol_v3_v4,
    &kia_protocol_v5,
    &kia_protocol_v6,
    &ford_protocol_v1,
    &ford_protocol_v2,
    &honda_static_protocol,
    &land_rover_v0_protocol,
    &mazda_v0_protocol,
    &kia_protocol_v7,
    &psa_protocol,
};

static const SubGhzProtocolRegistry protopirate_protocol_registry_fm = {
    .items = protopirate_protocol_registry_fm_items,
    .size = sizeof(protopirate_protocol_registry_fm_items) /
            sizeof(protopirate_protocol_registry_fm_items[0]),
};

static const ProtoPirateProtocolPlugin protopirate_fm_plugin = {
    .plugin_name = "ProtoPirate FM Registry",
    .filter = ProtoPirateProtocolRegistryFilterFM,
    .registry = &protopirate_protocol_registry_fm,
};

static const FlipperAppPluginDescriptor protopirate_fm_plugin_descriptor = {
    .appid = PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION,
    .entry_point = &protopirate_fm_plugin,
};

const FlipperAppPluginDescriptor* protopirate_fm_plugin_ep(void) {
    return &protopirate_fm_plugin_descriptor;
}
