#pragma once

#include "include/i18n/strings.h"
#include <stdbool.h>

bool settings_load_locale(ImpostorLocale *locale_out);
void settings_save_locale(ImpostorLocale locale);
