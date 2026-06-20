#pragma once

#include <stdbool.h>

typedef struct ProtoPirateApp ProtoPirateApp;

bool protopirate_psa_bf_plugin_ensure_loaded(ProtoPirateApp* app);
void protopirate_psa_bf_plugin_unload_if_idle(ProtoPirateApp* app);
void protopirate_psa_bf_context_release(ProtoPirateApp* app);

void protopirate_receiver_info_rebuild_normal_widget(void* app);

#ifdef ENABLE_SUB_DECODE_SCENE
void protopirate_subdecode_psa_bf_complete_refresh(void* app);
#endif
