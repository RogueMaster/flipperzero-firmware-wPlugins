#include "../fdxb_maker.h"

void fdxb_maker_scene_save_success_on_enter(void* context) {
    FdxbMaker* app = context;
    Popup* popup = app->popup;

    app->needs_restore = false;

    popup_set_icon(popup, 36, 5, &I_DolphinSaved_92x58);
    popup_set_header(popup, "Saved!", 15, 19, AlignLeft, AlignBottom);
    popup_set_context(popup, app);
    popup_set_callback(popup, fdxb_maker_popup_timeout_callback);
    popup_set_timeout(popup, 1500);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, FdxbMakerViewPopup);
}

bool fdxb_maker_scene_save_success_on_event(void* context, SceneManagerEvent event) {
    FdxbMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack || event.type == SceneManagerEventTypeCustom) {
        fdxb_maker_reset_data(app);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, FdxbMakerSceneSelectKey);
        consumed = true;
    }

    return consumed;
}

void fdxb_maker_scene_save_success_on_exit(void* context) {
    FdxbMaker* app = context;

    popup_reset(app->popup);
}
