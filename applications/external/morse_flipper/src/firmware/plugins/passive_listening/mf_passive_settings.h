#pragma once

#include "mf_passive_types.h"

typedef struct {
    MfPassiveSettingsModel model;
    MfPassiveSettingsArgs settings;
    VariableItem* items[9];
    bool active;
    bool dirty;
    bool save_failed;
} MfPassiveSettingsState;

bool mf_passive_settings_enter(
    MfPassiveSettingsState* state,
    const MfPassiveEnterArgs* args,
    MfPassiveResult* result);
void mf_passive_settings_leave(MfPassiveSettingsState* state);
