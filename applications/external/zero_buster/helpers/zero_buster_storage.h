#pragma once

#include "zero_buster.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Save Custom HID configuration to MicroSD */
void save_custom_hid_config(Storage* storage, const FuriHalUsbHidConfig* cfg);

/* Load or generate default Custom HID config from MicroSD */
void load_custom_hid_config(Storage* storage, FuriHalUsbHidConfig* cfg);

/* Ensure default demonstration payloads exist on MicroSD and are up to date */
void ensure_default_payloads(Storage* storage);

/* Scan /ext/zero_buster/payloads for available files */
void scan_payloads(ZeroBusterApp* app);

/* Generate next incremental loot filepath in /ext/zero_buster/loot/ */
void get_next_loot_filepath(Storage* storage, char* out_path, size_t max_len);

#ifdef __cplusplus
}
#endif
