#include "../uk_mbirth_sonicare.h"
#include "../sonicare_password.h"
#include <gui/canvas.h>
#include <gui/modules/widget.h>
#include <gui/modules/widget_elements/widget_element.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <uk_mbirth_sonicare_icons.h>
#include <dolphin/dolphin.h>

void format_bytes(FuriString* str, const uint8_t* data, size_t size) {
    for(size_t i = 0; i < size; i++) {
        furi_string_cat_printf(str, " %02X", data[i]);
    }
}

void format_bytes_reverse(FuriString* str, const uint8_t* data, size_t size) {
    for(size_t i = size; i > 0; i--) {
        furi_string_cat_printf(str, " %02X", data[i-1]);
    }
}

void sonicare_scene_read_complete_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Sonicare* app = context;
    if (type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void sonicare_scene_read_complete_on_enter(void* context) {
    Sonicare* app = context;
    Widget* widget = app->widget;
    
    widget_reset(widget);
    
    const NfcDevice* nfc_device = app->nfc_device;
    FURI_LOG_D("sonicare_scene_read_complete", "Pulling Mifare Ultralight data from NFC device");
    const MfUltralightData* ul_data = app->nfc_data;

    FURI_LOG_D("sonicare_scene_read_complete", "Alloc'ing temp_str for output");
    FuriString* temp_str = furi_string_alloc();

    furi_string_cat_printf(temp_str, "\e#%s\n", nfc_device_get_name(nfc_device, NfcDeviceNameTypeFull));
    
    // UID
    furi_string_cat_printf(temp_str, "UID:");
    format_bytes(temp_str, ul_data->iso14443_3a_data->uid, ul_data->iso14443_3a_data->uid_len);
    furi_string_cat_printf(temp_str, "\n");
    
    // Manufacturing Code
    furi_string_cat_str(temp_str, "MFG: ");
    FuriString* serial_no = furi_string_alloc();
    furi_string_cat_str(serial_no, (char*)(ul_data->page[0x21].data));
    furi_string_right(serial_no, 2);
    furi_string_cat(temp_str, serial_no);
    furi_string_cat_printf(temp_str, "\n");

    // Usage
    uint16_t seconds = ul_data->page[0x24].data[1]*256 + ul_data->page[0x24].data[0];
    uint16_t brushes = seconds / 120;   // one brush = 2 minutes
    if (seconds % 120 > 0) {
        // The official SoniCare app shows "remaining brushes" and if there's not at least
        // 2 minutes remaining, it doesn't count as a brush. So, let's include
        // unfinished brushes in the count.
        brushes++;
    }
    const uint8_t hours = seconds / 3600;
    seconds %= 3600;
    const uint8_t minutes = seconds / 60;
    seconds %= 60;
    furi_string_cat_printf(temp_str, "Usage: %u brushes (%u:%02u:%02u)\n", (int)brushes, (int)hours, (int)minutes, (int)seconds);

    // Max Usage
    uint16_t max_seconds = ul_data->page[0x21].data[1]*256 + ul_data->page[0x21].data[0];
    const uint16_t max_brushes = max_seconds / 120;
    const uint8_t max_hours = max_seconds / 3600;
    max_seconds %= 3600;
    const uint8_t max_minutes = max_seconds / 60;
    max_seconds %= 60;
    furi_string_cat_printf(temp_str, "Max: %u brushes (%u:%02u:%02u)\n", (int)max_brushes, (int)max_hours, (int)max_minutes, (int)max_seconds);

    // TODO: Maybe show "replace soon" marker if 170 brushes or more?
    // TODO: Maybe show "replace head" marker if 180 brushes or more?

    // NFC password
    uint32_t unlock_pwd_big = get_sonicare_password((uint8_t*)ul_data->iso14443_3a_data->uid, (uint8_t*)furi_string_get_cstr(serial_no));
    FURI_LOG_D("sonicare_scene_read_complete", "NFC unlock password: 0x%08lx", unlock_pwd_big);
    uint8_t unlock_pwd[4];
    memcpy(unlock_pwd, &unlock_pwd_big, sizeof(unlock_pwd_big));
    furi_string_cat_printf(temp_str, "NFC unlock:");
    format_bytes_reverse(temp_str, unlock_pwd, 4);
    furi_string_cat_printf(temp_str, "\n");

    // Output to widget
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);
    furi_string_free(serial_no);

    // TODO: widget_add_button_element(widget, GuiButtonTypeRight, "Change", sonicare_scene_read_complete_widget_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewWidget);
}

bool sonicare_scene_read_complete_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);

    bool consumed = false;
    
    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == GuiButtonTypeRight) {
            // switch to edit screen
            consumed = true;
        }
    } else if (event.type == SceneManagerEventTypeBack) {
        // Back button pressed
        //consumed = true;
    }
    
    return consumed;
}

void sonicare_scene_read_complete_on_exit(void* context) {
    Sonicare* app = context;
    widget_reset(app->widget);
}
