#pragma once

#include <storage/storage.h>
#include "keyboard_settings.h"

bool clipboard_keyboard_settings_load(Storage* storage, ClipboardKeyboardSettings* settings);
bool clipboard_keyboard_settings_save(Storage* storage, const ClipboardKeyboardSettings* settings);
