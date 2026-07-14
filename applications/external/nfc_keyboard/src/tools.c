#include "tools.h"

// https://github.com/flipperdevices/flipperzero-firmware/blob/7291e6bd46c564400cdd3e9e7434f2c6e3a5ff28/applications/main/nfc/scenes/nfc_scene_delete.c#L27

void convertToHexString(FuriString* str, const uint8_t* uid, size_t length) {
    for(size_t i = 0; i < length; i++) {
        furi_string_cat_printf(str, "%02X", uid[i]);
        if(i < (length - 1)) {
            furi_string_cat_printf(str, " ");
        }
    }
}
