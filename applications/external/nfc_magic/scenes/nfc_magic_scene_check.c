#include "../nfc_magic_app_i.h"

void nfc_magic_check_worker_callback(NfcMagicScannerEvent event, void* context) {
    furi_assert(context);

    NfcMagicApp* instance = context;

    if(event.type == NfcMagicScannerEventTypeDetected) {
        instance->protocol = event.data.protocol;
        instance->gen2_type = event.data.gen2_type;
        instance->gen1_uid_len = event.data.gen1_uid_len;
        instance->uscuid_ul_data = event.data.uscuid_ul;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerSuccess);
    } else if(event.type == NfcMagicScannerEventTypeDetectedNotMagic) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerFail);
    }
}

void nfc_magic_scene_check_on_enter(void* context) {
    NfcMagicApp* instance = context;

    // Fresh scan: drop any password / wipe mode armed for a previously scanned tag.
    instance->uscuid_ul_password_set = false;
    instance->uscuid_ul_is_wipe_mode = false;
    // The MFC dict attack caches the keys it finds in target_dev (its on_exit always stores
    // them). Clear it so this scan re-reads the card in front of us instead of replaying the
    // previous tag's keys -- stale keys would auth nothing yet still report a clean wipe.
    nfc_device_clear(instance->target_dev);

    popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
    popup_set_text(instance->popup, "Apply card to\nthe back", 128, 32, AlignRight, AlignCenter);

    nfc_magic_app_blink_start(instance);

    nfc_magic_scanner_start(instance->scanner, nfc_magic_check_worker_callback, instance);
    nfc_magic_scanner_set_gen4_password(instance->scanner, instance->gen4_password);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewPopup);
}

bool nfc_magic_scene_check_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcMagicCustomEventWorkerSuccess) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneMagicInfo);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerFail) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneNotMagic);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_magic_scene_check_on_exit(void* context) {
    NfcMagicApp* instance = context;

    nfc_magic_scanner_stop(instance->scanner);
    popup_reset(instance->popup);
    nfc_magic_app_blink_stop(instance);
}
