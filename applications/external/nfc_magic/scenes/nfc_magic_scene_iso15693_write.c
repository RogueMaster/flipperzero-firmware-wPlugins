#include "../nfc_magic_app_i.h"
#include "../magic/protocols/iso15693/iso15693_poller.h"

static void
    nfc_magic_scene_iso15693_write_poller_callback(Iso15693PollerEvent event, void* context) {
    NfcMagicApp* instance = context;

    if(event == Iso15693PollerEventSuccess) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerSuccess);
    } else if(event == Iso15693PollerEventCardLost) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcMagicCustomEventCardLost);
    } else { // Iso15693PollerEventFail: card present but backdoor write not accepted (not magic)
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerFail);
    }
}

void nfc_magic_scene_iso15693_write_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Popup* popup = instance->popup;

    popup_set_header(popup, "Writing UID", 68, 19, AlignCenter, AlignBottom);
    popup_set_text(popup, "Keep card on the\nback of Flipper", 68, 21, AlignCenter, AlignTop);
    popup_set_icon(popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewPopup);

    // Allocate the poller here (not at app startup); freed in on_exit.
    instance->iso15693_poller = iso15693_poller_alloc(instance->nfc);
    iso15693_poller_start_write_uid(
        instance->iso15693_poller,
        instance->iso15693_target_uid,
        nfc_magic_scene_iso15693_write_poller_callback,
        instance);
    nfc_magic_app_blink_start(instance);
}

bool nfc_magic_scene_iso15693_write_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcMagicCustomEventWorkerSuccess) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneSuccess);
            consumed = true;
        } else if(
            event.event == NfcMagicCustomEventWorkerFail ||
            event.event == NfcMagicCustomEventCardLost) {
            // ISO15693 has its own fail scene with a reason, instead of the generic write-fail: a
            // rejected backdoor write means "not a magic tag", not a transient error.
            const uint32_t reason = (event.event == NfcMagicCustomEventCardLost) ?
                                        NfcMagicIso15693WriteFailReasonCardLost :
                                        NfcMagicIso15693WriteFailReasonNotMagic;
            scene_manager_set_scene_state(
                instance->scene_manager, NfcMagicSceneIso15693WriteFail, reason);
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneIso15693WriteFail);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_magic_scene_iso15693_write_on_exit(void* context) {
    NfcMagicApp* instance = context;
    iso15693_poller_stop(instance->iso15693_poller);
    iso15693_poller_free(instance->iso15693_poller);
    instance->iso15693_poller = NULL;
    nfc_magic_app_blink_stop(instance);
    popup_reset(instance->popup);
}
