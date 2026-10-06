#pragma once
#include <furi.h>
#include <storage/storage.h>
#include "alerts.h"

#define PHR_SETTINGS_PATH APP_DATA_PATH("settings.conf")

typedef enum {
    PhrTransportBle = 0,
    PhrTransportUsb = 1,
    PhrTransportCount
} PhrTransportId;

typedef struct {
    uint8_t transport; // PhrTransportId
} PhrSettings;

void phr_settings_defaults(PhrSettings* s, AlertEngine* alerts);
/** Loads settings; missing/corrupt files or values fall back to defaults. */
void phr_settings_load(Storage* storage, PhrSettings* s, AlertEngine* alerts);
bool phr_settings_save(Storage* storage, const PhrSettings* s, const AlertEngine* alerts);
