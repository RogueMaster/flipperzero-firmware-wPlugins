#include "../nfc_magic_app_i.h"

enum {
    NfcMagicSceneDumpStateCardSearch,
    NfcMagicSceneDumpStateCardFound,
};

NfcCommand nfc_magic_scene_dump_gen1_poller_callback(Gen1aPollerEvent event, void* context) {
    NfcMagicApp* instance = context;
    furi_assert(event.data);

    NfcCommand command = NfcCommandContinue;

    if(event.type == Gen1aPollerEventTypeDetected) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventCardDetected);
    } else if(event.type == Gen1aPollerEventTypeRequestMode) {
        event.data->request_mode.mode = Gen1aPollerModeDump;
    } else if(event.type == Gen1aPollerEventTypeRequestDataToDump) {
        event.data->data_to_dump.mfc_data = instance->dump_data;
    } else if(event.type == Gen1aPollerEventTypeSuccess) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerSuccess);
        command = NfcCommandStop;
    } else if(event.type == Gen1aPollerEventTypeFail) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerFail);
        command = NfcCommandStop;
    }

    return command;
}

static void nfc_magic_scene_dump_setup_view(NfcMagicApp* instance) {
    Popup* popup = instance->popup;
    popup_reset(popup);
    uint32_t state = scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneDump);

    if(state == NfcMagicSceneDumpStateCardSearch) {
        popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
        popup_set_text(
            instance->popup, "Apply the\nsame card\nto the back", 128, 32, AlignRight, AlignCenter);
    } else {
        popup_set_icon(popup, 12, 23, &I_Loading_24);
        popup_set_header(popup, "Dumping\nDon't move...", 52, 32, AlignLeft, AlignCenter);
    }

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewPopup);
}

void nfc_magic_scene_dump_on_enter(void* context) {
    NfcMagicApp* instance = context;

    scene_manager_set_scene_state(
        instance->scene_manager, NfcMagicSceneDump, NfcMagicSceneDumpStateCardSearch);
    nfc_magic_scene_dump_setup_view(instance);

    nfc_magic_app_blink_start(instance);

    if(instance->protocol == NfcMagicProtocolGen1) {
        instance->gen1a_poller = gen1a_poller_alloc(instance->nfc);
        gen1a_poller_start(
            instance->gen1a_poller, nfc_magic_scene_dump_gen1_poller_callback, instance);
    }
}

bool nfc_magic_scene_dump_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcMagicCustomEventCardDetected) {
            scene_manager_set_scene_state(
                instance->scene_manager, NfcMagicSceneDump, NfcMagicSceneDumpStateCardFound);
            nfc_magic_scene_dump_setup_view(instance);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventCardLost) {
            scene_manager_set_scene_state(
                instance->scene_manager, NfcMagicSceneDump, NfcMagicSceneDumpStateCardSearch);
            nfc_magic_scene_dump_setup_view(instance);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerSuccess) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneGen1SaveName);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerFail) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneDumpFail);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_magic_scene_dump_on_exit(void* context) {
    NfcMagicApp* instance = context;

    if(instance->protocol == NfcMagicProtocolGen1) {
        gen1a_poller_stop(instance->gen1a_poller);
        gen1a_poller_free(instance->gen1a_poller);
    }

    nfc_device_set_data(instance->source_dev, NfcProtocolMfClassic, instance->dump_data);

    scene_manager_set_scene_state(
        instance->scene_manager, NfcMagicSceneDump, NfcMagicSceneDumpStateCardSearch);
    // Clear view
    popup_reset(instance->popup);

    nfc_magic_app_blink_stop(instance);
}
