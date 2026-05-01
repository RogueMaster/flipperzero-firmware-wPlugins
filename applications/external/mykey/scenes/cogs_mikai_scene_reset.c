#include "../cogs_mikai.h"

static void cogs_mikai_scene_reset_popup_callback(void* context) {
    COGSMyKaiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void cogs_mikai_scene_reset_on_enter(void* context) {
    COGSMyKaiApp* app = context;
    Popup* popup = app->popup;

    if(!app->mykey.is_loaded) {
        popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, "No card loaded\nRead a card first", 64, 25, AlignCenter, AlignTop);
        popup_set_callback(popup, cogs_mikai_scene_reset_popup_callback);
        popup_set_context(popup, app);
        popup_set_timeout(popup, 2000);
        popup_enable_timeout(popup);
        view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);
        return;
    }

    // Reset the card
    mykey_reset(&app->mykey);

    // Update cached values
    app->mykey.is_reset = mykey_is_reset(&app->mykey);
    app->mykey.current_credit = mykey_get_current_credit(&app->mykey);

    // Show confirmation - saved in memory, not written to card
    popup_set_header(popup, "Card Reset!", 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, "Reset in memory\nUse 'Write to Card'", 64, 25, AlignCenter, AlignTop);
    popup_set_callback(popup, cogs_mikai_scene_reset_popup_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 2000);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);
    notification_message(app->notifications, &sequence_success);
}

bool cogs_mikai_scene_reset_on_event(void* context, SceneManagerEvent event) {
    COGSMyKaiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        // Search back to start scene and switch (forces menu rebuild)
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, COGSMyKaiSceneStart);
    }

    return consumed;
}

void cogs_mikai_scene_reset_on_exit(void* context) {
    COGSMyKaiApp* app = context;
    popup_reset(app->popup);
}
