#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ProtoPirateApp ProtoPirateApp;
typedef struct ProtoPirateTxRx ProtoPirateTxRx;

void protopirate_unload_protocol_plugin(ProtoPirateTxRx* txrx);
bool protopirate_refresh_protocol_registry(ProtoPirateApp* app, bool ensure_receiver_ready);
bool protopirate_apply_protocol_registry_for_context(
    ProtoPirateApp* app,
    const char* preset_name,
    uint32_t frequency,
    const uint8_t* preset_data,
    size_t preset_data_size,
    const char* protocol_name);
