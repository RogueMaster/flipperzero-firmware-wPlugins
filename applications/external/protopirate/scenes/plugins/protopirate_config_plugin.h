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

typedef struct ProtoPirateApp ProtoPirateApp;
typedef struct ProtoPirateConfigSceneHostApi {
    bool (*protopirate_refresh_protocol_registry)(ProtoPirateApp* app, bool ensure_receiver_ready);
    void (*protopirate_preset_init)(
        void* context,
        const char* preset_name,
        uint32_t frequency,
        uint8_t* preset_data,
        size_t preset_data_size);
} ProtoPirateConfigSceneHostApi;

typedef struct ProtoPirateConfigPlugin {
    const char* plugin_name;
    bool (*car_model_get_by_index)(
        ProtoPirateCarModel* car_model,
        uint16_t index,
        uint16_t model_count,
        SubGhzSetting* app_settings);
    uint16_t (*car_model_get_count)(void);
    void (*on_enter)(void* app);
    void (*set_host_api)(const ProtoPirateConfigSceneHostApi* host_api);
} ProtoPirateConfigPlugin;
