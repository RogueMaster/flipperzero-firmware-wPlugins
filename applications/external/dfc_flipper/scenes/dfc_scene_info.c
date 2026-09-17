#include "../dfc_i.h"
#include <dolphin/dolphin.h>

#define TAG "DfcSceneInfo"

static uint8_t empty[DFC_MAX_KEY_LEN] = {0};

void dfc_scene_info_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Dfc* dfc = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(dfc->view_dispatcher, result);
    }
}

void dfc_scene_info_on_enter(void* context) {
    Dfc* dfc = context;
    Widget* widget = dfc->widget;
    DfcCredential* credential = dfc->credential;
    const DfcApplication* app = dfc_credential_get_primary_application_const(credential);

    FuriString* primary_str = furi_string_alloc_set("Info");
    FuriString* uid_str = furi_string_alloc();
    FuriString* aid_str = furi_string_alloc();
    FuriString* details_str = furi_string_alloc();

    furi_string_cat_printf(uid_str, "UID: ");
    for(size_t i = 0; i < credential->uid_len; i++) {
        furi_string_cat_printf(uid_str, "%02X", credential->uid[i]);
    }

    if(app) {
        furi_string_cat_printf(
            aid_str, "AID: %02X%02X%02X", app->aid[0], app->aid[1], app->aid[2]);
    } else {
        furi_string_cat_printf(aid_str, "AID: none");
    }

    furi_string_cat_printf(
        details_str, "%zu app(s), %zu file(s)", credential->num_apps, credential->num_files);
    if(app) {
        for(size_t i = 0; i < app->num_keys; i++) {
            const uint8_t* key = dfc_credential_key_const(credential, app, i);
            if(key && memcmp(key, empty, app->key_len) != 0) {
                furi_string_cat_printf(details_str, " +keys");
                break;
            }
        }
    } else {
        const uint8_t* key = dfc_credential_key_const(credential, NULL, 0);
        if(key && memcmp(key, empty, credential->picc_key_len) != 0) {
            furi_string_cat_printf(details_str, " +keys");
        }
    }

    widget_add_string_element(
        widget, 64, 5, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(primary_str));

    widget_add_string_element(
        widget, 64, 20, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(uid_str));

    widget_add_string_element(
        widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(aid_str));

    widget_add_string_element(
        widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(details_str));

    furi_string_free(primary_str);
    furi_string_free(uid_str);
    furi_string_free(aid_str);
    furi_string_free(details_str);
    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewWidget);
}

bool dfc_scene_info_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(dfc->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(dfc->scene_manager);
    }
    return consumed;
}

void dfc_scene_info_on_exit(void* context) {
    Dfc* dfc = context;

    // Clear view
    widget_reset(dfc->widget);
}
