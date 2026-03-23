#include "../seos_i.h"
#include <dolphin/dolphin.h>

#define TAG "SeosSceneMigrateKeys"

void seos_scene_migrate_keys_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Seos* seos = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(seos->view_dispatcher, result);
    }
}

void seos_scene_migrate_keys_on_enter(void* context) {
    Seos* seos = context;
    Widget* widget = seos->widget;

    FuriString* temp_str = furi_string_alloc_printf(
        "Migrate %s from v1 to v2, which features per-device encryption",
        furi_string_get_cstr(seos->active_key_file));

    widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(temp_str));

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Skip", seos_scene_migrate_keys_widget_callback, seos);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Migrate", seos_scene_migrate_keys_widget_callback, seos);

    furi_string_free(temp_str);
    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewWidget);
}

bool seos_scene_migrate_keys_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            seos_migrate_keys(seos);
            scene_manager_search_and_switch_to_previous_scene(
                seos->scene_manager, SeosSceneMainMenu);
            consumed = true;
        } else if(event.event == GuiButtonTypeLeft) {
            // Skip migration, keep v1 keys loaded
            scene_manager_search_and_switch_to_previous_scene(
                seos->scene_manager, SeosSceneMainMenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(seos->scene_manager, SeosSceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void seos_scene_migrate_keys_on_exit(void* context) {
    Seos* seos = context;

    // Clear view
    widget_reset(seos->widget);
}
