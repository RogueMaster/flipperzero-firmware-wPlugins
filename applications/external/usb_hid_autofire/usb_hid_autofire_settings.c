#include "usb_hid_autofire_i.h"

void usb_hid_autofire_settings_save_timer_callback(void* ctx) {
    UsbHidAutofireApp* app = ctx;
    UsbMouseEvent event = {.type = EventTypeSettingsSave};
    furi_message_queue_put(app->event_queue, &event, 0);
}

void usb_hid_autofire_mark_settings_dirty(UsbHidAutofireApp* app) {
    app->settings_dirty = true;
    if(app->settings_save_timer) {
        furi_timer_start(app->settings_save_timer, furi_ms_to_ticks(SETTINGS_SAVE_DEBOUNCE_MS));
    }
}

static bool usb_hid_autofire_settings_save(const UsbHidAutofireApp* app) {
    bool success = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* settings_file = flipper_format_file_alloc(storage);
    if(settings_file &&
       flipper_format_file_open_always(settings_file, USB_HID_AUTOFIRE_SETTINGS_PATH)) {
        do {
            if(!flipper_format_write_header_cstr(
                   settings_file,
                   USB_HID_AUTOFIRE_SETTINGS_FILE_TYPE,
                   USB_HID_AUTOFIRE_SETTINGS_VERSION)) {
                break;
            }

            uint32_t delay_ms = usb_hid_autofire_delay_clamp(app->autofire_delay_ms);
            uint32_t mode = app->mode;
            uint32_t preset = app->preset;
            uint32_t startup_policy = app->startup_policy;
            bool last_active = app->last_active_state;

            if(!flipper_format_write_uint32(settings_file, "delay_ms", &delay_ms, 1)) break;
            if(!flipper_format_write_uint32(settings_file, "mode", &mode, 1)) break;
            if(!flipper_format_write_uint32(settings_file, "preset", &preset, 1)) break;
            if(!flipper_format_write_uint32(settings_file, "startup_policy", &startup_policy, 1))
                break;
            if(!flipper_format_write_bool(settings_file, "last_active", &last_active, 1)) break;

            success = true;
        } while(false);
    }

    if(settings_file) {
        flipper_format_free(settings_file);
    }
    furi_record_close(RECORD_STORAGE);

    if(!success) {
        FURI_LOG_W(TAG, "Failed to save settings");
    }

    return success;
}

bool usb_hid_autofire_settings_load(UsbHidAutofireApp* app) {
    bool loaded = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* settings_file = flipper_format_file_alloc(storage);
    FuriString* file_type = furi_string_alloc();
    uint32_t version = 0;
    uint32_t delay_ms = AUTOFIRE_DELAY_DEFAULT_MS;
    uint32_t mode = AutofireModeMouseLeftClick;
    uint32_t preset = AutofirePresetCustom;
    uint32_t startup_policy = AutofireStartupPolicyPausedOnLaunch;
    bool last_active = false;

    if(settings_file &&
       flipper_format_file_open_existing(settings_file, USB_HID_AUTOFIRE_SETTINGS_PATH)) {
        do {
            if(!flipper_format_read_header(settings_file, file_type, &version)) break;
            if((strcmp(furi_string_get_cstr(file_type), USB_HID_AUTOFIRE_SETTINGS_FILE_TYPE) !=
                0) ||
               (version != USB_HID_AUTOFIRE_SETTINGS_VERSION))
                break;
            if(!flipper_format_read_uint32(settings_file, "delay_ms", &delay_ms, 1)) break;
            if(!flipper_format_read_uint32(settings_file, "mode", &mode, 1)) break;
            if(!flipper_format_read_uint32(settings_file, "preset", &preset, 1)) break;
            if(!flipper_format_read_uint32(settings_file, "startup_policy", &startup_policy, 1))
                break;
            if(!flipper_format_read_bool(settings_file, "last_active", &last_active, 1)) break;
            if(!usb_hid_autofire_mode_is_valid(mode)) break;
            if(!usb_hid_autofire_preset_is_valid(preset)) break;
            if(!usb_hid_autofire_startup_policy_is_valid(startup_policy)) break;

            loaded = true;
        } while(false);
    }

    furi_string_free(file_type);
    if(settings_file) {
        flipper_format_free(settings_file);
    }
    furi_record_close(RECORD_STORAGE);

    if(!loaded) {
        return false;
    }

    app->autofire_delay_ms = usb_hid_autofire_delay_clamp(delay_ms);
    app->mode = (AutofireMode)mode;
    app->preset = (AutofirePreset)preset;
    app->startup_policy = AutofireStartupPolicyPausedOnLaunch;
    app->last_active_state = last_active;

    if((app->preset != AutofirePresetCustom) &&
       (app->autofire_delay_ms != usb_hid_autofire_preset_delay_ms(app->preset))) {
        app->preset = AutofirePresetCustom;
    }

    return true;
}

void usb_hid_autofire_settings_flush_if_dirty(UsbHidAutofireApp* app) {
    if(!app->settings_dirty) {
        return;
    }

    if(usb_hid_autofire_settings_save(app)) {
        app->settings_dirty = false;
    }
}
