#include "../sniffer_card_app.h"

#include <sniffer_card_icons.h>

static void sniffer_card_app_scene_write_uid_poller_callback(void* context) {
    SnifferCardApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SnifferCardAppCustomEventPollerDone);
}

void sniffer_card_app_scene_write_uid_on_enter(void* context) {
    SnifferCardApp* app = context;

    popup_reset(app->popup);
    popup_set_icon(app->popup, 0, 0, &I_NFC_dolphin_emulation_51x64);
    popup_set_header(app->popup, "Setting UID", 90, 10, AlignCenter, AlignTop);
    // "Still" earns its line: the write power-cycles the card to read the UID back, and a card
    // lifted during that reports as unconfirmed rather than as written.
    popup_set_text(app->popup, "Hold the\nsniffer card\nstill", 90, 26, AlignCenter, AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewPopup);

    // The UID was handed to the poller by the screen before this one, which is the screen that
    // knew which card it came off.
    sniffer_card_poller_start(
        app->poller,
        SnifferCardPollerModeWriteUid,
        sniffer_card_app_scene_write_uid_poller_callback,
        app);
}

bool sniffer_card_app_scene_write_uid_on_event(void* context, SceneManagerEvent event) {
    SnifferCardApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != SnifferCardAppCustomEventPollerDone) return false;

    sniffer_card_app_take_result(app);

    notification_message(
        app->notifications,
        app->result == SnifferCardPollerResultSuccess ? &sequence_success : &sequence_error);

    scene_manager_next_scene(app->scene_manager, SnifferCardAppSceneWriteResult);

    return true;
}

void sniffer_card_app_scene_write_uid_on_exit(void* context) {
    SnifferCardApp* app = context;

    sniffer_card_poller_stop(app->poller);
    popup_reset(app->popup);
}
