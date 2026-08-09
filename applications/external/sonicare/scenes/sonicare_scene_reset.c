#include "../uk_mbirth_sonicare.h"
#include "../sonicare_password.h"
#include <uk_mbirth_sonicare_icons.h>

#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <dolphin/dolphin.h>

#define SONICARE_PAGE_USAGE 0x24

static NfcCommand sonicare_scene_reset_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    Sonicare* app = context;

    if(event.protocol != NfcProtocolMfUltralight) {
        return NfcCommandContinue;
    }

    const MfUltralightPollerEvent* ev = event.event_data;
    NfcCommand command = NfcCommandContinue;

    if(ev->type == MfUltralightPollerEventTypeRequestMode) {
        ev->data->poller_mode = MfUltralightPollerModeRead;
    } else if(ev->type == MfUltralightPollerEventTypeAuthRequest) {
        // The poller asks us for the password before reading protected pages.
        // Provide it (MSB-first, as in the 8-digit hex string).
        uint32_t pwd = get_sonicare_password(app->sonicare_uid, app->sonicare_mfg);
        FURI_LOG_I("sonicare_scene_reset", "AuthRequest - providing password %08lX", pwd);
        ev->data->auth_context.password.data[0] = (pwd >> 24) & 0xFF;
        ev->data->auth_context.password.data[1] = (pwd >> 16) & 0xFF;
        ev->data->auth_context.password.data[2] = (pwd >> 8) & 0xFF;
        ev->data->auth_context.password.data[3] = (pwd >> 0) & 0xFF;
        ev->data->auth_context.skip_auth = false;
    } else if(ev->type == MfUltralightPollerEventTypeAuthSuccess) {
        FURI_LOG_I("sonicare_scene_reset", "Auth success, writing page 0x24");

        MfUltralightPoller* poller = (MfUltralightPoller*)event.instance;

        // Official reset: write page 0x24 = {00 00 02 00}
        MfUltralightPage page = {.data = {0x00, 0x00, 0x02, 0x00}};
        MfUltralightError err =
            mf_ultralight_poller_write_page(poller, SONICARE_PAGE_USAGE, &page);

        if(err == MfUltralightErrorNone) {
            FURI_LOG_I("sonicare_scene_reset", "Usage counter reset OK");
            app->reset_state = SonicareResetStateSuccess;
        } else {
            FURI_LOG_E("sonicare_scene_reset", "Write failed: %d", err);
            app->reset_state = SonicareResetStateFailedWrite;
        }

        view_dispatcher_send_custom_event(app->view_dispatcher, NfcCustomEventWorkerExit);
        command = NfcCommandStop;
    } else if(ev->type == MfUltralightPollerEventTypeAuthFailed) {
        FURI_LOG_E("sonicare_scene_reset", "Auth failed (poller state machine)");
        app->reset_state = SonicareResetStateFailedAuth;
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcCustomEventWorkerExit);
        command = NfcCommandStop;
    } else if(ev->type == MfUltralightPollerEventTypeReadSuccess) {
        // Card has no password protection or auth was skipped; try direct write.
        FURI_LOG_I("sonicare_scene_reset", "ReadSuccess without prior auth, trying write");
        MfUltralightPoller* poller = (MfUltralightPoller*)event.instance;
        MfUltralightPage page = {.data = {0x00, 0x00, 0x02, 0x00}};
        MfUltralightError err =
            mf_ultralight_poller_write_page(poller, SONICARE_PAGE_USAGE, &page);
        app->reset_state = (err == MfUltralightErrorNone) ? SonicareResetStateSuccess :
                                                            SonicareResetStateFailedWrite;
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcCustomEventWorkerExit);
        command = NfcCommandStop;
    }

    return command;
}

void sonicare_scene_reset_on_enter(void* context) {
    Sonicare* app = context;
    Popup* popup = app->popup;

    app->reset_state = SonicareResetStateInit;

    popup_reset(popup);
    popup_set_header(popup, "Resetting", 83, 8, AlignCenter, AlignTop);
    popup_set_text(
        popup, "Hold brush stem\nnext to\nFlipper's back", 83, 27, AlignCenter, AlignTop);
    popup_set_icon(app->popup, 0, 0, &I_sonicare_read);
    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewPopup);

    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(app->poller, sonicare_scene_reset_poller_callback, app);
}

bool sonicare_scene_reset_on_event(void* context, SceneManagerEvent event) {
    Sonicare* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcCustomEventWorkerExit) {
            if(app->reset_state == SonicareResetStateSuccess) {
                notification_message(app->notifications, &sequence_success);
                dolphin_deed(DolphinDeedNfcRead);
            } else {
                notification_message(app->notifications, &sequence_error);
            }
            scene_manager_next_scene(app->scene_manager, SonicareSceneResetComplete);
            consumed = true;
        }
    }

    return consumed;
}

void sonicare_scene_reset_on_exit(void* context) {
    Sonicare* app = context;

    nfc_poller_stop(app->poller);
    nfc_poller_free(app->poller);
    popup_reset(app->popup);
}
