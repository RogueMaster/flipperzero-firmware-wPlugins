/**
 * Per-app persisted settings.
 *
 * Stored at /ext/apps_data/claude_buddy/settings.bin (1 byte — version + mode).
 * Accessed from the GUI thread only.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* Custom Flipper-UUID serial profile, talks to the Python host-bridge
     * (existing `plugin/host-bridge/` stack). */
    BleModeBridge = 0,
    /* Nordic UART Service profile, advertises "Claude" so Claude Desktop /
     * Claude Cowork in Developer Mode can pair directly (no host bridge). */
    BleModeDesktop = 1,
} BleMode;

#define APP_SETTINGS_NAME_MAX    32
#define APP_SETTINGS_DEVNAME_MAX 18  /* matches FURI_HAL_VERSION_DEVICE_NAME_LENGTH */

BleMode app_settings_get_ble_mode(void);
/* Returns true on successful write. */
bool app_settings_set_ble_mode(BleMode mode);

/* Copies the persisted owner name (from cmd:owner) into out, or an empty
 * string if never set.  Returns true iff the stored name is non-empty. */
bool app_settings_get_owner_name(char* out, int out_size);
void app_settings_set_owner_name(const char* name);

/* Same for the device's advertised BLE name (from cmd:name).  Empty =
 * use the default "Claude Flipper". */
bool app_settings_get_device_name(char* out, int out_size);
void app_settings_set_device_name(const char* name);

#ifdef __cplusplus
}
#endif
