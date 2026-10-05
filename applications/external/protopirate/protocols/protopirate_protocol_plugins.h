#pragma once

#include <lib/subghz/types.h>
#include "protocol_items.h"

#define PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID      "protopirate_protocol_plugins"
#define PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION ((uint32_t)sizeof(ProtoPirateProtocolPlugin))

typedef enum {
    ProtoPirateProtocolPluginKindRx = 0,
    ProtoPirateProtocolPluginKindTx,
} ProtoPirateProtocolPluginKind;

typedef struct {
    const char* plugin_name;
    ProtoPirateProtocolPluginKind kind;
    ProtoPirateProtocolRegistryRoute route;
    const char* protocol_name;
    const SubGhzProtocolRegistry* registry;
    void (*release)(void);
} ProtoPirateProtocolPlugin;
