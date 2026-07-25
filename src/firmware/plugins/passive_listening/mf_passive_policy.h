#pragma once

#include "mf_passive_types.h"

void mf_passive_settings_normalize(MfPassiveSettingsModel* model);
uint8_t mf_passive_settings_wpm(const MfPassiveSettingsModel* model);
const char* mf_passive_settings_lesson_charset(void);
void mf_passive_settings_load(MfPassiveSettingsModel* model);
bool mf_passive_settings_save(const MfPassiveSettingsModel* model);
