/* The settings file on the SD card, through saved_struct. Saving creates
 * APP_SETTINGS_PARENT and APP_SETTINGS_DIR first, as a fresh SD card may have
 * neither. */
#pragma once

#include "app_settings.h"

extern const AppSettingsStorage app_settings_file;
