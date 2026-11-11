#include "../badge_wallet.h"

/* Emulate the selected card by driving the NFC listener directly.
 *
 * Lifecycle mirrors nfc_playlist's emulate step exactly:
 *   nfc_alloc -> nfc_device_alloc -> nfc_device_load ->
 *   nfc_device_get_protocol -> nfc_listener_alloc(nfc, proto,
 *   nfc_device_get_data(dev, proto)) -> nfc_listener_start.
 * On exit everything is stopped and freed, releasing the NFC HAL so the app is
 * only ever holding the radio while this scene is on screen. This is app + NFC
 * only (the ST25R3916 IC), independent of the BLE stack, so it is brick-safe.
 */

void badge_wallet_scene_emulate_on_enter(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    BadgeCard* card = &app->cards[app->selected_index];

    popup_reset(app->popup);
    popup_set_header(app->popup, "Emulating", 64, 12, AlignCenter, AlignTop);

    app->nfc = nfc_alloc();
    app->emulate_device = nfc_device_alloc();

    bool started = false;
    if(nfc_device_load(app->emulate_device, furi_string_get_cstr(card->path))) {
        NfcProtocol protocol = nfc_device_get_protocol(app->emulate_device);
        if(protocol != NfcProtocolInvalid) {
            app->listener = nfc_listener_alloc(
                app->nfc, protocol, nfc_device_get_data(app->emulate_device, protocol));
            nfc_listener_start(app->listener, NULL, NULL);
            started = true;
        }
    }

    if(started) {
        snprintf(
            app->emulate_text,
            sizeof(app->emulate_text),
            "%s\nApply to reader",
            furi_string_get_cstr(card->label));
        notification_message(app->notifications, &sequence_blink_start_cyan);
    } else {
        /* Load failed or unsupported protocol: tear down the NFC we allocated
         * and tell the user, rather than pretending to emulate. */
        if(app->emulate_device) {
            nfc_device_free(app->emulate_device);
            app->emulate_device = NULL;
        }
        if(app->nfc) {
            nfc_free(app->nfc);
            app->nfc = NULL;
        }
        snprintf(
            app->emulate_text,
            sizeof(app->emulate_text),
            "Cannot emulate\n%s",
            furi_string_get_cstr(card->label));
    }

    popup_set_text(app->popup, app->emulate_text, 64, 34, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadgeWalletViewPopup);
}

bool badge_wallet_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    /* Back is handled by the view dispatcher's navigation callback, which pops
     * the scene and triggers on_exit (where emulation is stopped). */
    return false;
}

void badge_wallet_scene_emulate_on_exit(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;

    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        app->listener = NULL;
    }
    if(app->emulate_device) {
        nfc_device_free(app->emulate_device);
        app->emulate_device = NULL;
    }
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
