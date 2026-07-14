#pragma once

#include <stdbool.h>

typedef struct ProtoPirateApp ProtoPirateApp;

bool protopirate_radio_init(ProtoPirateApp* app);
void protopirate_radio_deinit(ProtoPirateApp* app);
