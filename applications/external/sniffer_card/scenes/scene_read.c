#include "../sniffer_card_app.h"

#include <sniffer_card_icons.h>

/** Called on the NFC worker thread when the session has a verdict.
 *
 * Nothing but the hand-off: the result is read back, and everything decided about it, on the GUI
 * thread in the event handler below.
 */
static void sniffer_card_app_scene_read_poller_callback(void* context) {
    SnifferCardApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SnifferCardAppCustomEventPollerDone);
}

void sniffer_card_app_scene_read_on_enter(void* context) {
    SnifferCardApp* app = context;

    popup_reset(app->popup);
    // Icon at the left edge with the text centred in what is left, the layout the firmware's own
    // NFC app uses with this asset (nfc_protocol_support.c:998). About fifteen characters a line
    // survive that, which is why the wording here is shorter than it reads elsewhere.
    popup_set_icon(app->popup, 0, 0, &I_NFC_dolphin_emulation_51x64);
    popup_set_header(app->popup, "Reading", 90, 10, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Hold the\nsniffer card\non the Flipper", 90, 26, AlignCenter, AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewPopup);

    sniffer_card_poller_start(
        app->poller,
        SnifferCardPollerModeReadTraces,
        sniffer_card_app_scene_read_poller_callback,
        app);
}

bool sniffer_card_app_scene_read_on_event(void* context, SceneManagerEvent event) {
    SnifferCardApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != SnifferCardAppCustomEventPollerDone) return false;

    sniffer_card_app_take_result(app);

    // Every verdict goes to the same screen, which is the only place that explains what the card
    // turned out to be: "not one of these cards" and "one of these cards with nothing on it yet"
    // need the same amount of saying, and splitting them across two screens only hides the
    // retry button from one of them.
    notification_message(
        app->notifications,
        app->result == SnifferCardPollerResultSuccess ? &sequence_success : &sequence_error);

    scene_manager_next_scene(app->scene_manager, SnifferCardAppSceneReadResult);

    return true;
}

void sniffer_card_app_scene_read_on_exit(void* context) {
    SnifferCardApp* app = context;

    sniffer_card_poller_stop(app->poller);
    popup_reset(app->popup);
}
