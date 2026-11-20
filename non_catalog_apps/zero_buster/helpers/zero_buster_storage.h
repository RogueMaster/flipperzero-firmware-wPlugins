#pragma once

#include "zero_buster.h"

#ifdef __cplusplus
extern "C" {
#endif

void save_custom_hid_config(Storage* storage, const FuriHalUsbHidConfig* cfg);

void load_custom_hid_config(Storage* storage, FuriHalUsbHidConfig* cfg);

void ensure_default_payloads(Storage* storage);

void scan_payloads(ZeroBusterApp* app);

/* Fills keymap with a stock BadUSB .kl table; falls back to built-in en-US.
   path == NULL or "" selects the built-in table explicitly. */
bool load_layout_keymap(Storage* storage, uint16_t* keymap, const char* path);

/* Lists /ext/badusb/assets/layouts and selects en-US by default. */
void scan_layouts(ZeroBusterApp* app);

/* Loads the keymap of the currently selected layout. */
void apply_selected_layout(ZeroBusterApp* app);

const char* current_layout_name(ZeroBusterApp* app);

void get_next_loot_filepath(Storage* storage, char* out_path, size_t max_len);

#ifdef __cplusplus
}
#endif
