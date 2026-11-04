#include "../sniffer_card_app.h"

static void sniffer_card_app_scene_enter_uid_callback(void* context) {
    SnifferCardApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SnifferCardAppCustomEventNext);
}

void sniffer_card_app_scene_enter_uid_on_enter(void* context) {
    SnifferCardApp* app = context;

    furi_check(app->uid_input_len <= sizeof(app->uid_input));

    byte_input_set_header_text(app->byte_input, "Enter the UID to write");
    byte_input_set_result_callback(
        app->byte_input,
        sniffer_card_app_scene_enter_uid_callback,
        NULL,
        app,
        app->uid_input,
        app->uid_input_len);

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewByteInput);
}

bool sniffer_card_app_scene_enter_uid_on_event(void* context, SceneManagerEvent event) {
    SnifferCardApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != SnifferCardAppCustomEventNext) return false;

    // A typed UID has no card behind it, so there is no SAK to carry and nothing after this reads
    // one: the write checks the sniffer card's own SAK and its own UID length, not the target's.
    memset(&app->target, 0, sizeof(app->target));
    memcpy(app->target.uid, app->uid_input, app->uid_input_len);
    app->target.uid_len = app->uid_input_len;

    sniffer_card_poller_set_uid(app->poller, app->target.uid, app->target.uid_len);

    // Straight to the write, skipping the confirm screen: that screen exists to show what was
    // read off a card, and there is nothing to confirm about a UID the user just typed.
    scene_manager_next_scene(app->scene_manager, SnifferCardAppSceneWriteUid);

    return true;
}

void sniffer_card_app_scene_enter_uid_on_exit(void* context) {
    SnifferCardApp* app = context;
    byte_input_set_result_callback(app->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(app->byte_input, "");
}
