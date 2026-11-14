#include "../nfc_magic_app_i.h"
#include "../magic/protocols/iso15693/iso15693_poller.h"

static void nfc_magic_iso15693_get_info_poller_callback(Iso15693PollerEvent event, void* context) {
    NfcMagicApp* instance = context;

    if(event == Iso15693PollerEventSuccess) {
        // On success, send a custom event to the scene manager to transition
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventIso15693CardDetected);
    } else if(event == Iso15693PollerEventCardLost) {
        // Info mode only ever emits Success or CardLost (never a write outcome). CardLost here means
        // no card activated within the poller's retry budget.
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventIso15693CardDetectFailed);
    }
    // Any other event is not expected in Info mode and is intentionally ignored.
}

// Back button on the "no card" fail screen; forwards to the scene event handler.
static void nfc_magic_iso15693_get_info_fail_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void nfc_magic_scene_iso15693_get_info_on_enter(void* context) {
    NfcMagicApp* app = context;
    Popup* popup = app->popup;

    // Icon on the left, text right-aligned to the right edge (like the scan / write popups) so it
    // clears the 60px-wide icon instead of overlapping it.
    popup_set_icon(popup, 0, 8, &I_NFC_manual_60x50);
    popup_set_text(popup, "Approach an\nISO15693 tag", 128, 32, AlignRight, AlignCenter);

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
            iso15693_3_copy(app->iso15693_data, iso15693_poller_get_data(app->iso15693_poller));
            scene_manager_next_scene(app->scene_manager, NfcMagicSceneIso15693Info);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventIso15693CardDetectFailed) {
            // Show a dolphin fail screen, same layout as the gen4 "no response" screen, rather than
            // silently dropping to the menu.
            nfc_magic_app_blink_stop(app);
            Widget* widget = app->widget;
            widget_add_icon_element(widget, 72, 17, &I_DolphinCommon_56x48);
            widget_add_string_element(
                widget, 7, 4, AlignLeft, AlignTop, FontPrimary, "No card found");
            widget_add_string_multiline_element(
                widget, 7, 17, AlignLeft, AlignTop, FontSecondary, "No ISO15693 tag\ndetected.");
            widget_add_button_element(
                widget,
                GuiButtonTypeLeft,
                "Back",
                nfc_magic_iso15693_get_info_fail_button_callback,
                app);
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcMagicAppViewWidget);
            consumed = true;
        } else if(event.event == GuiButtonTypeLeft) {
            // "Back" on the fail screen -> return to the ISO15693 menu.
            consumed = scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, NfcMagicSceneIso15693);
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

    // Reset both views (detect uses the popup; the "no card" fail screen uses the widget).
    popup_reset(app->popup);
    widget_reset(app->widget);
}
