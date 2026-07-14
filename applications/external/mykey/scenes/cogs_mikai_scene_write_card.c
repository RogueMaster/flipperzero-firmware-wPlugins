#include "../cogs_mikai.h"

static void cogs_mikai_scene_write_card_popup_callback(void* context) {
    COGSMyKaiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void cogs_mikai_scene_write_card_on_enter(void* context) {
    COGSMyKaiApp* app = context;
    Popup* popup = app->popup;

    if(!app->mykey.is_loaded) {
        popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, "No card loaded", 64, 25, AlignCenter, AlignTop);
        popup_set_callback(popup, cogs_mikai_scene_write_card_popup_callback);
        popup_set_context(popup, app);
        popup_set_timeout(popup, 2000);
        popup_enable_timeout(popup);
        view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);
        return;
    }

    if(!app->mykey.is_modified) {
        popup_set_header(popup, "No Changes", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, "Card data not modified", 64, 25, AlignCenter, AlignTop);
        popup_set_callback(popup, cogs_mikai_scene_write_card_popup_callback);
        popup_set_context(popup, app);
        popup_set_timeout(popup, 2000);
        popup_enable_timeout(popup);
        view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);
        return;
    }

    popup_set_header(popup, "Writing...", 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, "Place card on reader", 64, 25, AlignCenter, AlignTop);
    popup_set_callback(popup, cogs_mikai_scene_write_card_popup_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 5000);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);

    if(mykey_write_to_nfc(app)) {
        app->mykey.is_modified = false;

        popup_set_header(popup, "Success!", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, "Card updated", 64, 25, AlignCenter, AlignTop);
        notification_message(app->notifications, &sequence_success);
    } else {
        popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, "Write failed\nTry again", 64, 25, AlignCenter, AlignTop);
        notification_message(app->notifications, &sequence_error);
    }

    popup_set_timeout(popup, 2000);
    popup_enable_timeout(popup);
}

bool cogs_mikai_scene_write_card_on_event(void* context, SceneManagerEvent event) {
    COGSMyKaiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, COGSMyKaiSceneStart);
        consumed = true;
    }

    return consumed;
}

void cogs_mikai_scene_write_card_on_exit(void* context) {
    COGSMyKaiApp* app = context;
    popup_reset(app->popup);
}
