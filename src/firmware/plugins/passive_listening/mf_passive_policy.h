#pragma once

#include "mf_passive_types.h"

void mf_passive_settings_normalize(MfPassiveSettingsModel* model);
uint8_t mf_passive_settings_wpm(const MfPassiveSettingsModel* model);
size_t mf_passive_settings_lesson_count(void);
uint8_t mf_passive_settings_lesson_charset_len(uint8_t lesson);
const char* mf_passive_settings_lesson_charset(void);
void mf_passive_settings_lesson_label(uint8_t lesson, char* out, size_t out_size);
void mf_passive_settings_load(MfPassiveSettingsModel* model);
bool mf_passive_settings_save(const MfPassiveSettingsModel* model);
