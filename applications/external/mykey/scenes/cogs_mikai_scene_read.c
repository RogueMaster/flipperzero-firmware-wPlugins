#include "../cogs_mikai.h"

static void cogs_mikai_scene_read_popup_callback(void* context) {
    COGSMyKaiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void cogs_mikai_scene_read_on_enter(void* context) {
    COGSMyKaiApp* app = context;

    // Show popup for card detection
    Popup* popup = app->popup;
    popup_set_header(popup, "Reading Card", 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, "Place COGES MyKey\non Flipper's back", 64, 25, AlignCenter, AlignTop);
    popup_set_icon(popup, 0, 0, NULL);
    popup_set_callback(popup, cogs_mikai_scene_read_popup_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 3000);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);

    // Attempt to read NFC card
    if(mykey_read_from_nfc(app)) {
        // Calculate encryption key
        mykey_calculate_encryption_key(&app->mykey);
        app->mykey.is_loaded = true;
        app->mykey.is_modified = false;  // Fresh read from card
        app->mykey.is_reset = mykey_is_reset(&app->mykey);
        app->mykey.current_credit = mykey_get_current_credit(&app->mykey);

        popup_set_header(popup, "Success!", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, "Card read successfully", 64, 25, AlignCenter, AlignTop);
        notification_message(app->notifications, &sequence_success);
    } else {
        popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, "Failed to read card\nsegmentaion fault", 64, 25, AlignCenter, AlignTop);
        notification_message(app->notifications, &sequence_error);
    }
}

bool cogs_mikai_scene_read_on_event(void* context, SceneManagerEvent event) {
    COGSMyKaiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        scene_manager_previous_scene(app->scene_manager);
    }

    return consumed;
}

void cogs_mikai_scene_read_on_exit(void* context) {
    COGSMyKaiApp* app = context;
    popup_reset(app->popup);
}
