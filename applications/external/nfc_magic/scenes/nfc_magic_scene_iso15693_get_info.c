#include "../nfc_magic_app_i.h"
#include "../magic/protocols/iso15693/iso15693_poller.h"

static void nfc_magic_iso15693_get_info_poller_callback(Iso15693PollerEvent event, void* context) {
    NfcMagicApp* instance = context;

    if(event == Iso15693PollerEventSuccess) {
        // On success, send a custom event to the scene manager to transition
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventIso15693CardDetected);
    } else { // Iso15693PollerEventFail
        // On failure, send a different event to go back
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventIso15693CardDetectFailed);
    }
}

void nfc_magic_scene_iso15693_get_info_on_enter(void* context) {
    NfcMagicApp* app = context;
    Popup* popup = app->popup;

    // Setup the popup view to instruct the user
    popup_set_header(popup, "Detecting ISO15693", 68, 19, AlignCenter, AlignBottom);
    popup_set_text(popup, "Approach card to the back of Flipper", 68, 21, AlignCenter, AlignTop);
    popup_set_icon(popup, 0, 8, &I_NFC_manual_60x50);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcMagicAppViewPopup);

    // Allocate the poller here (not at app startup) so it doesn't hold the shared Nfc's
    // config across the scanner's run. Freed in on_exit.
    app->iso15693_poller = iso15693_poller_alloc(app->nfc);
    iso15693_poller_start(app->iso15693_poller, nfc_magic_iso15693_get_info_poller_callback, app);
    nfc_magic_app_blink_start(app);
}

bool nfc_magic_scene_iso15693_get_info_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcMagicCustomEventIso15693CardDetected) {
            // Keep the read result in the app so the info scene still has it after the
            // poller is freed in on_exit.
            iso15693_data_copy(app->iso15693_data, iso15693_poller_get_data(app->iso15693_poller));
            scene_manager_next_scene(app->scene_manager, NfcMagicSceneIso15693Info);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventIso15693CardDetectFailed) {
            // Failed to detect, go back to the previous scene (the ISO15693 menu)
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, NfcMagicSceneIso15693);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_magic_scene_iso15693_get_info_on_exit(void* context) {
    NfcMagicApp* app = context;

    iso15693_poller_stop(app->iso15693_poller);
    iso15693_poller_free(app->iso15693_poller);
    app->iso15693_poller = NULL;
    nfc_magic_app_blink_stop(app);

    // Reset the popup to a clean state for the next view
    popup_reset(app->popup);
}
