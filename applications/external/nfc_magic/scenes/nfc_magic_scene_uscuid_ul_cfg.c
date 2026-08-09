#include "../nfc_magic_app_i.h"

void nfc_magic_scene_uscuid_ul_cfg_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    widget_add_string_element(widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Config");

    const uint8_t* cfg = instance->uscuid_ul_data.config;
    FuriString* message = furi_string_alloc();

    // Raw 16-byte config page (4/line with offset), then decoded fields. Byte 5 ("mystery")
    // and the type/preset byte are not decoded; version bytes are shown raw.
    for(size_t i = 0; i < USCUID_UL_CONFIG_SIZE; i += 4) {
        furi_string_cat_printf(
            message,
            "%02X: %02X %02X %02X %02X\n",
            (unsigned)i,
            cfg[i],
            cfg[i + 1],
            cfg[i + 2],
            cfg[i + 3]);
    }
    furi_string_cat_str(message, "--\n");

    furi_string_cat_printf(
        message,
        "Wakeup: %s, %s\n",
        ((cfg[0] == 0x7A) && (cfg[1] == 0xFF)) ? "on" : "off",
        (cfg[2] == 0x85) ? "20/23" : "40/43");
    furi_string_cat_printf(message, "Cfg by RATS: %s\n", (cfg[3] == 0xA0) ? "on" : "off");
    furi_string_cat_printf(message, "Auth: %s\n", (cfg[4] == 0x00) ? "PWD" : "2TDEA");
    furi_string_cat_printf(message, "CUID: %s\n", (cfg[6] == 0x0A) ? "on" : "off");
    furi_string_cat_str(message, "Version:\n");
    for(size_t i = 8; i < USCUID_UL_CONFIG_SIZE; i++) {
        furi_string_cat_printf(message, "%02X%s", cfg[i], (i == 11) ? "\n" : " ");
    }

    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(message));
    furi_string_free(message);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_uscuid_ul_cfg_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(instance->scene_manager);
    }
    return consumed;
}

void nfc_magic_scene_uscuid_ul_cfg_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
