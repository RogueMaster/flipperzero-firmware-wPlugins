#include "../seos_i.h"
#include <dolphin/dolphin.h>

#define TAG "SeosSceneInfo"

static uint8_t empty[16] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void seos_scene_info_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Seos* seos = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(seos->view_dispatcher, result);
    }
}

void seos_scene_info_on_enter(void* context) {
    Seos* seos = context;
    Widget* widget = seos->widget;
    SeosCredential* credential = seos->credential;

    FuriString* primary_str = furi_string_alloc_set("Info");
    FuriString* secondary_str_label = furi_string_alloc();
    FuriString* secondary_str_value = furi_string_alloc();
    FuriString* details_str = furi_string_alloc();
    FuriString* keys_str = furi_string_alloc();

    furi_string_set(secondary_str_label, "Diversifier:");
    for(size_t i = 0; i < credential->diversifier_len; i++) {
        furi_string_cat_printf(secondary_str_value, "%02X", credential->diversifier[i]);
    }

    // RID
    if(credential->sio_len > 3 && credential->sio[2] == 0x81) {
        size_t len = credential->sio[3];
        furi_string_set(details_str, "RID:");
        for(size_t i = 0; i < len; i++) {
            furi_string_cat_printf(details_str, "%02X", credential->sio[4 + i]);
        }
        if(len >= 4 && credential->sio[3 + 1] == 0x01) {
            furi_string_cat_printf(details_str, "(retail)");
        } else if(len == 2) {
            furi_string_cat_printf(details_str, "(ER)");
        } else {
            furi_string_cat_printf(details_str, "(other)");
        }
    }

    // keys
    if(memcmp(credential->priv_key, empty, sizeof(empty)) != 0) {
        furi_string_cat_printf(keys_str, "+keys");
    }

    widget_add_string_element(
        widget, 64, 5, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(primary_str));

    widget_add_string_element(
        widget,
        64,
        20,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(secondary_str_label));

    widget_add_string_element(
        widget,
        64,
        30,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(secondary_str_value));

    widget_add_string_element(
        widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(details_str));

    widget_add_string_element(
        widget, 64, 50, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(keys_str));

    furi_string_free(primary_str);
    furi_string_free(secondary_str_label);
    furi_string_free(secondary_str_value);
    furi_string_free(details_str);
    furi_string_free(keys_str);
    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewWidget);
}

bool seos_scene_info_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(seos->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(seos->scene_manager);
    }
    return consumed;
}

void seos_scene_info_on_exit(void* context) {
    Seos* seos = context;

    // Clear view
    widget_reset(seos->widget);
}
