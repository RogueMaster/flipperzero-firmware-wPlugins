#pragma once

#include "helpers/protopirate_models.h"
#include <lib/flipper_application/flipper_application.h>
#include "helpers/variable_item_list.h"

#define PROTOPIRATE_CONFIG_PLUGIN_APP_ID      "protopirate_config_plugin"
#define PROTOPIRATE_CONFIG_PLUGIN_API_VERSION 2U

enum ProtoPirateSettingIndex {
    ProtoPirateSettingIndexCarModel,
    ProtoPirateSettingIndexFrequency,
    ProtoPirateSettingIndexHopping,
    ProtoPirateSettingIndexModulation,
#ifdef ENABLE_EMULATE_FEATURE
    ProtoPirateSettingIndexTXPower,
#endif
    ProtoPirateSettingIndexAutoSave,
    ProtoPirateSettingIndexDateTimeFilenames,
    ProtoPirateSettingIndexSound,
    ProtoPirateSettingIndexCheckSaved,
    ProtoPirateSettingIndexLock,
};

typedef struct ProtoPirateConfigPlugin {
    const char* plugin_name;
    bool (*car_model_get_by_index)(
        ProtoPirateCarModel* car_model,
        uint16_t index,
        uint16_t model_count,
        SubGhzSetting* app_settings);
    uint16_t (*car_model_get_count)(void);
    void (*on_enter)(void* app);
} ProtoPirateConfigPlugin;
