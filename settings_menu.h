#pragma once

#include "nfc_canary_i.h"

/* One row of the settings screen. Values are edited in place with Left/Right,
 * so every setting is a small enum or bounded integer -- no text entry, no
 * sub-menus. Keeps the whole config one screen deep. */
typedef struct {
    const char* label;
    uint8_t* field; /* points into AlerterSettings */
    uint8_t max; /* inclusive upper bound */
    const char* (*fmt)(uint8_t value); /* render value, NULL = numeric */
} SettingRow;

/* Build the row table against a settings struct. Returns row count. */
uint8_t settings_menu_build(AlerterSettings* s, SettingRow* rows, uint8_t cap);
