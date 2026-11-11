#include "namespoof.h"

#include <flipper_format/flipper_format.h>
#include <furi_hal_version.h>
#include <furi_hal_version_i.h>

#define TAG "NameSpoof"

// Whether the previous namespoof_init() pass applied a MAC / serial override.
// Mirrors the existing prev_custom_name handling for the SD-remount revert path.
static bool prev_mac_override = false;
static bool prev_serial_override = false;

static bool namespoof_all_zero(const uint8_t* data, size_t size) {
    for(size_t i = 0; i < size; i++) {
        if(data[i]) return false;
    }
    return true;
}

void namespoof_init(void) {
    FuriString* str = furi_string_alloc();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    const char* prev_custom_name = version_get_custom_name(NULL);
    bool mac_ok = false;
    bool serial_ok = false;
    uint8_t mac[NAMESPOOF_MAC_LEN];
    uint8_t uid[NAMESPOOF_SERIAL_LEN];

    // Detach the old name before freeing it, even when only Mac/Serial remain.
    // Rebuild the real identity first so removed overrides cannot survive a reload.
    if(prev_custom_name || prev_mac_override || prev_serial_override) {
        version_set_custom_name(NULL, NULL);
        furi_hal_version_init();
    }

    do {
        uint32_t version;
        if(!flipper_format_file_open_existing(file, NAMESPOOF_PATH)) break;
        if(!flipper_format_read_header(file, str, &version)) break;
        if(furi_string_cmp_str(str, NAMESPOOF_HEADER)) break;
        if(version != NAMESPOOF_VERSION) break;

        // All three keys are optional and read independently (rewind before each,
        // so file order does not matter). A missing Name must NOT skip Mac/Serial.

        // Name first: furi_hal_version_set_name() re-derives ble_mac + custom_uid,
        // so the explicit overrides below have to be applied after it.
        flipper_format_rewind(file);
        if(flipper_format_read_string(file, NAMESPOOF_KEY_NAME, str) &&
           furi_string_size(str) > 0) {
            version_set_custom_name(NULL, strdup(furi_string_get_cstr(str)));
            furi_hal_version_set_name(NULL);
        }

        // BLE MAC: all-zero is the "real" sentinel (matches the UI) = not set.
        flipper_format_rewind(file);
        if(flipper_format_read_hex(file, NAMESPOOF_KEY_MAC, mac, NAMESPOOF_MAC_LEN) &&
           !namespoof_all_zero(mac, NAMESPOOF_MAC_LEN)) {
            mac_ok = true;
        }

        // Serial / UID: same all-zero sentinel.
        flipper_format_rewind(file);
        if(flipper_format_read_hex(file, NAMESPOOF_KEY_SERIAL, uid, NAMESPOOF_SERIAL_LEN) &&
           !namespoof_all_zero(uid, NAMESPOOF_SERIAL_LEN)) {
            serial_ok = true;
        }

        if(mac_ok) furi_hal_version_set_ble_mac(mac);
        if(serial_ok) furi_hal_version_set_custom_uid(uid);
    } while(false);

    if(prev_custom_name) free((void*)prev_custom_name);

    prev_mac_override = mac_ok;
    prev_serial_override = serial_ok;

    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(str);
}
