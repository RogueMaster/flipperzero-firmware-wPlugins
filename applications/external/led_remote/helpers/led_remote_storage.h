#pragma once

#include "../led_remote.h"

void led_remote_storage_save(LedRemoteApp* app);
void led_remote_storage_load(LedRemoteApp* app);

void led_remote_settings_save(LedRemoteApp* app);
void led_remote_settings_load(LedRemoteApp* app);
