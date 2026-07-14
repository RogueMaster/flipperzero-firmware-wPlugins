#include "../mizip_balance_editor_i.h"

void mizip_balance_editor_scene_write_success_popup_callback(void* context) {
    MiZipBalanceEditorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MiZipBalanceEditorCustomEventViewExit);
}

void mizip_balance_editor_scene_write_success_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;

    popup_set_icon(app->popup, 36, 5, &I_DolphinSaved_92x58);
    popup_set_header(app->popup, "Saved", 15, 19, AlignLeft, AlignBottom);
    popup_set_timeout(app->popup, 2000);
    popup_set_context(app->popup, context);
    popup_set_callback(app->popup, mizip_balance_editor_scene_write_success_popup_callback);
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdWriteSuccess);
}

bool mizip_balance_editor_scene_write_success_on_event(void* context, SceneManagerEvent event) {
    MiZipBalanceEditorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MiZipBalanceEditorCustomEventViewExit) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MiZipBalanceEditorViewIdMainMenu);
        }
    }
    return consumed;
}

void mizip_balance_editor_scene_write_success_on_exit(void* context) {
    MiZipBalanceEditorApp* app = context;
    popup_reset(app->popup);
}
