#include "../protopirate_protocol_plugins.h"
#include "../protocols_common.h"
#include "../chrysler_v0.h"
#include "../fiat_v0.h"
#include "../fiat_v1.h"
#include "../fiat_v2.h"
#include "../ford_v0.h"
#include "../ford_v3.h"
#include "../kia_v1.h"
#include "../kia_v2.h"
#include "../mazda_v0.h"
#include "../porsche_touareg.h"
#include "../psa.h"
#include "../renault_v0.h"
#include "../subaru.h"
#include "../star_line.h"
#include "../honda_v1.h"

static const SubGhzProtocol* const protopirate_protocol_registry_am_items[] = {
    &chrysler_protocol_v0,
    &fiat_protocol_v0,
    &fiat_v1_protocol,
    &fiat_v2_protocol,
    &ford_protocol_v0,
    &ford_protocol_v3,
    &honda_v1_protocol,
    &kia_protocol_v1,
    &kia_protocol_v2,
    &mazda_v0_protocol,
    &porsche_touareg_protocol,
    &psa_protocol,
    &renault_v0_protocol,
    &subaru_protocol,
    &subghz_protocol_star_line,
};

static const SubGhzProtocolRegistry protopirate_protocol_registry_am = {
    .items = protopirate_protocol_registry_am_items,
    .size = sizeof(protopirate_protocol_registry_am_items) /
            sizeof(protopirate_protocol_registry_am_items[0]),
};

static const ProtoPirateProtocolPlugin protopirate_am_plugin = {
    .plugin_name = "ProtoPirate AM Default Registry",
    .kind = ProtoPirateProtocolPluginKindRx,
    .route = ProtoPirateProtocolRegistryRouteAMDefault,
    .registry = &protopirate_protocol_registry_am,
    .release = NULL,
};

static const FlipperAppPluginDescriptor protopirate_am_plugin_descriptor = {
    .appid = PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION,
    .entry_point = &protopirate_am_plugin,
};

const FlipperAppPluginDescriptor* protopirate_am_plugin_ep(void) {
    return &protopirate_am_plugin_descriptor;
}
