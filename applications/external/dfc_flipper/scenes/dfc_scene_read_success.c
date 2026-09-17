#include "../dfc_i.h"
#include <dolphin/dolphin.h>

#define TAG "DfcSceneReadCardSuccess"

void dfc_scene_read_success_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Dfc* dfc = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(dfc->view_dispatcher, result);
    }
}

void dfc_scene_read_success_on_enter(void* context) {
    Dfc* dfc = context;
    DfcCredential* credential = dfc->credential;
    const DfcApplication* app = dfc_credential_get_primary_application_const(credential);
    Widget* widget = dfc->widget;

    dolphin_deed(DolphinDeedNfcReadSuccess);

    FuriString* primary_str = furi_string_alloc_set("Card Read");
    FuriString* uid_str = furi_string_alloc();
    FuriString* details_str = furi_string_alloc();

    furi_string_cat_printf(uid_str, "UID: ");
    for(size_t i = 0; i < credential->uid_len; i++) {
        furi_string_cat_printf(uid_str, "%02X", credential->uid[i]);
    }

    if(app) {
        furi_string_cat_printf(
            details_str,
            "AID %02X%02X%02X, %zu file(s)",
            app->aid[0],
            app->aid[1],
            app->aid[2],
            credential->num_files);
    } else {
        furi_string_cat_printf(
            details_str, "%zu app(s), %zu file(s)", credential->num_apps, credential->num_files);
    }

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Save", dfc_scene_read_success_widget_callback, dfc);

    widget_add_button_element(
        widget, GuiButtonTypeRight, "Emulate", dfc_scene_read_success_widget_callback, dfc);

    widget_add_string_element(
        widget, 64, 5, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(primary_str));

    widget_add_string_element(
        widget, 64, 20, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(uid_str));

    widget_add_string_element(
        widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(details_str));

    furi_string_free(primary_str);
    furi_string_free(uid_str);
    furi_string_free(details_str);
    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewWidget);
}

bool dfc_scene_read_success_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            scene_manager_next_scene(dfc->scene_manager, DfcSceneSaveName);
            consumed = true;
        } else if(event.event == GuiButtonTypeRight) {
            scene_manager_next_scene(dfc->scene_manager, DfcSceneSavedMenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(dfc->scene_manager, DfcSceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void dfc_scene_read_success_on_exit(void* context) {
    Dfc* dfc = context;

    // Clear view
    widget_reset(dfc->widget);
}
