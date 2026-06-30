#include "../protopirate_protocol_plugins.h"
#include "../chrysler_v0.h"
#include "../fiat_v0.h"
#include "../fiat_v1.h"
#include "../ford_v0.h"
#include "../kia_v1.h"
#include "../porsche_touareg.h"
#include "../psa.h"
#include "../subaru.h"
#include "../vag.h"
#include "../star_line.h"

static const SubGhzProtocol* const protopirate_protocol_registry_am_items[] = {
    &chrysler_protocol_v0,
    &fiat_protocol_v0,
    &fiat_v1_protocol,
    &ford_protocol_v0,
    &kia_protocol_v1,
    &porsche_touareg_protocol,
    &psa_protocol,
    &subaru_protocol,
    &vag_protocol,
    &subghz_protocol_star_line,
};

static const SubGhzProtocolRegistry protopirate_protocol_registry_am = {
    .items = protopirate_protocol_registry_am_items,
    .size = sizeof(protopirate_protocol_registry_am_items) /
            sizeof(protopirate_protocol_registry_am_items[0]),
};

static const ProtoPirateProtocolPlugin protopirate_am_plugin = {
    .plugin_name = "ProtoPirate AM Registry",
    .filter = ProtoPirateProtocolRegistryFilterAM,
    .registry = &protopirate_protocol_registry_am,
};

static const FlipperAppPluginDescriptor protopirate_am_plugin_descriptor = {
    .appid = PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION,
    .entry_point = &protopirate_am_plugin,
};

const FlipperAppPluginDescriptor* protopirate_am_plugin_ep(void) {
    return &protopirate_am_plugin_descriptor;
}
