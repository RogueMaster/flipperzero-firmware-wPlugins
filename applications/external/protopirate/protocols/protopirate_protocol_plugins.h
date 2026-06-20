#pragma once

#include <lib/flipper_application/flipper_application.h>
#include <lib/subghz/types.h>
#include "protocol_items.h"

#define PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID      "protopirate_protocol_plugins"
#define PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION 1U

typedef struct {
    const char* plugin_name;
    ProtoPirateProtocolRegistryFilter filter;
    const SubGhzProtocolRegistry* registry;
} ProtoPirateProtocolPlugin;
