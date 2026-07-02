#pragma once

#include <lib/flipper_application/flipper_application.h>
#include <lib/subghz/types.h>
#include "protocol_items.h"

#define PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID      "protopirate_protocol_plugins"
#define PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION 2U

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
