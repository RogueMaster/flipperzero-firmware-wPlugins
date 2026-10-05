#pragma once
#include <furi.h>
#include "../defines.h"

#ifdef ENABLE_MODELS_DATABASE
#include <lib/subghz/types.h>
#include "core/record.h"
#include <lib/subghz/subghz_setting.h>

typedef struct ProtoPirateCarModel {
    char* name;
    SubGhzRadioPreset* preset;
    uint16_t index;
    int16_t last_preset_index;
} ProtoPirateCarModel;

bool car_model_get_by_index(
    ProtoPirateCarModel* car_model,
    uint16_t index,
    uint16_t model_count,
    SubGhzSetting* app_settings);

uint16_t car_model_get_count(void);
#endif
