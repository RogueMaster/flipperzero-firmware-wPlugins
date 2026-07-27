#include "../nfc_magic_app_i.h"

enum {
    NfcMagicSceneWriteStateCardSearch,
    NfcMagicSceneWriteStateCardFound,
};

NfcCommand nfc_magic_scene_write_gen1_poller_callback(Gen1aPollerEvent event, void* context) {
    NfcMagicApp* instance = context;
    furi_assert(event.data);

    NfcCommand command = NfcCommandContinue;

    if(event.type == Gen1aPollerEventTypeDetected) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventCardDetected);
    } else if(event.type == Gen1aPollerEventTypeRequestMode) {
        event.data->request_mode.mode = Gen1aPollerModeWrite;
    } else if(event.type == Gen1aPollerEventTypeRequestDataToWrite) {
        const MfClassicData* mfc_data =
            nfc_device_get_data(instance->source_dev, NfcProtocolMfClassic);
        event.data->data_to_write.mfc_data = mfc_data;
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

NfcCommand nfc_magic_scene_write_gen2_poller_callback(Gen2PollerEvent event, void* context) {
    NfcMagicApp* instance = context;
    furi_assert(event.data);

    NfcCommand command = NfcCommandContinue;

    if(event.type == Gen2PollerEventTypeDetected) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventCardDetected);
    } else if(event.type == Gen2PollerEventTypeRequestMode) {
        event.data->poller_mode.mode = Gen2PollerModeWrite;
    } else if(event.type == Gen2PollerEventTypeRequestDataToWrite) {
        const MfClassicData* mfc_data =
            nfc_device_get_data(instance->source_dev, NfcProtocolMfClassic);
        event.data->data_to_write.mfc_data = mfc_data;
    } else if(event.type == Gen2PollerEventTypeRequestTargetData) {
        const MfClassicData* mfc_data =
            nfc_device_get_data(instance->target_dev, NfcProtocolMfClassic);
        event.data->target_data.mfc_data = mfc_data;
    } else if(event.type == Gen2PollerEventTypeSuccess) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerSuccess);
        command = NfcCommandStop;
    } else if(event.type == Gen2PollerEventTypePartial) {
        instance->gen2_partial_blocks_total = event.data->partial.blocks_total;
        instance->gen2_partial_failed_count = event.data->partial.failed_count;
        memcpy(
            instance->gen2_partial_failed_bitmap,
            event.data->partial.failed_bitmap,
            sizeof(instance->gen2_partial_failed_bitmap));
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerPartial);
        command = NfcCommandStop;
    } else if(event.type == Gen2PollerEventTypeFail) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerFail);
        command = NfcCommandStop;
    }

    return command;
}

NfcCommand nfc_magic_scene_write_gen4_poller_callback(Gen4PollerEvent event, void* context) {
    NfcMagicApp* instance = context;
    furi_assert(event.data);

    NfcCommand command = NfcCommandContinue;

    if(event.type == Gen4PollerEventTypeCardDetected) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventCardDetected);
    } else if(event.type == Gen4PollerEventTypeRequestMode) {
        event.data->request_mode.mode = Gen4PollerModeWrite;
    } else if(event.type == Gen4PollerEventTypeRequestDataToWrite) {
        NfcProtocol protocol = nfc_device_get_protocol(instance->source_dev);
        event.data->request_data.protocol = protocol;
        event.data->request_data.data = nfc_device_get_data(instance->source_dev, protocol);
    } else if(event.type == Gen4PollerEventTypeSuccess) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerSuccess);
        command = NfcCommandStop;
    } else if(event.type == Gen4PollerEventTypeFail) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerFail);
        command = NfcCommandStop;
    }

    return command;
}

NfcCommand
    nfc_magic_scene_write_uscuid_ul_poller_callback(UscuidUlPollerEvent event, void* context) {
    NfcMagicApp* instance = context;
    furi_assert(event.data);

    NfcCommand command = NfcCommandContinue;

    if(event.type == UscuidUlPollerEventTypeDetected) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventCardDetected);
    } else if(event.type == UscuidUlPollerEventTypeRequestMode) {
        event.data->poller_mode.mode = instance->uscuid_ul_is_wipe_mode ? UscuidUlPollerModeWipe :
                                                                          UscuidUlPollerModeWrite;
    } else if(event.type == UscuidUlPollerEventTypeRequestDataToWrite) {
        event.data->data_to_write.data =
            nfc_device_get_data(instance->source_dev, NfcProtocolMfUltralight);
    } else if(event.type == UscuidUlPollerEventTypeWriteProgress) {
        instance->write_progress_current = event.data->write_progress.pages_written;
        instance->write_progress_total = event.data->write_progress.pages_total;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerProgress);
    } else if(event.type == UscuidUlPollerEventTypeSuccess) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerSuccess);
        command = NfcCommandStop;
    } else if(event.type == UscuidUlPollerEventTypePartial) {
        instance->write_progress_current = event.data->partial.pages_written;
        instance->write_progress_total = event.data->partial.pages_total;
        instance->write_failed_count = event.data->partial.failed_count;
        memcpy(
            instance->write_failed_bitmap,
            event.data->partial.failed_bitmap,
            sizeof(instance->write_failed_bitmap));
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerPartial);
        command = NfcCommandStop;
    } else if(event.type == UscuidUlPollerEventTypeFail) {
        instance->write_progress_current = event.data->fail.pages_written;
        instance->write_progress_total = event.data->fail.pages_total;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerFail);
        command = NfcCommandStop;
    } else if(event.type == UscuidUlPollerEventTypeAuthFailed) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerAuthFail);
        command = NfcCommandStop;
    }

    return command;
}

// ISO15693 clone: our iso15693 poller uses a simpler event callback than the SDK-style pollers
// above. Map its outcome onto the shared write events; the per-block partial stats are stashed for
// the fail/partial screen.
static void
    nfc_magic_scene_write_iso15693_poller_callback(Iso15693PollerEvent event, void* context) {
    NfcMagicApp* instance = context;

    if(event == Iso15693PollerEventSuccess) {
        // Read the clone stats on success too, so the Success handler can distinguish an exact clone
        // from one where the card ended up advertising more blocks than it physically holds
        // (over-capacity with empty tail -- no data lost, but worth a note).
        iso15693_poller_get_clone_result(
            instance->iso15693_poller,
            &instance->iso15693_clone_blocks_total,
            &instance->iso15693_clone_failed_count,
            &instance->iso15693_clone_over_capacity,
            instance->iso15693_clone_failed_bitmap,
            &instance->iso15693_clone_used_gen1);
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerSuccess);
    } else if(event == Iso15693PollerEventPartial) {
        iso15693_poller_get_clone_result(
            instance->iso15693_poller,
            &instance->iso15693_clone_blocks_total,
            &instance->iso15693_clone_failed_count,
            &instance->iso15693_clone_over_capacity,
            instance->iso15693_clone_failed_bitmap,
            &instance->iso15693_clone_used_gen1);
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerPartial);
    } else if(event == Iso15693PollerEventCardLost) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcMagicCustomEventCardLost);
    } else { // Iso15693PollerEventFail: backdoor write not accepted (not a magic tag)
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerFail);
    }
}

static void nfc_magic_scene_write_setup_view(NfcMagicApp* instance) {
    Popup* popup = instance->popup;
    popup_reset(popup);
    uint32_t state = scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneWrite);

    if(state == NfcMagicSceneWriteStateCardSearch) {
        popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
        popup_set_text(
            instance->popup, "Apply the\nsame card\nto the back", 128, 32, AlignRight, AlignCenter);
    } else {
        popup_set_icon(popup, 12, 23, &I_Loading_24);
        popup_set_header(
            popup,
            instance->uscuid_ul_is_wipe_mode ? "Wiping\nDon't move..." : "Writing\nDon't move...",
            52,
            32,
            AlignLeft,
            AlignCenter);
    }

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewPopup);
}

void nfc_magic_scene_write_on_enter(void* context) {
    NfcMagicApp* instance = context;

    instance->write_progress_current = 0;
    instance->write_progress_total = 0;
    instance->write_failed_count = 0;

    scene_manager_set_scene_state(
        instance->scene_manager, NfcMagicSceneWrite, NfcMagicSceneWriteStateCardSearch);
    nfc_magic_scene_write_setup_view(instance);

    nfc_magic_app_blink_start(instance);

    if(instance->protocol == NfcMagicProtocolGen1) {
        instance->gen1a_poller = gen1a_poller_alloc(instance->nfc);
        gen1a_poller_start(
            instance->gen1a_poller, nfc_magic_scene_write_gen1_poller_callback, instance);
    } else if(instance->protocol == NfcMagicProtocolGen2) {
        instance->gen2_poller = gen2_poller_alloc(instance->nfc);
        gen2_poller_start(
            instance->gen2_poller, nfc_magic_scene_write_gen2_poller_callback, instance);
    } else if(instance->protocol == NfcMagicProtocolClassic) {
        instance->gen2_poller = gen2_poller_alloc(instance->nfc);
        gen2_poller_start(
            instance->gen2_poller, nfc_magic_scene_write_gen2_poller_callback, instance);
    } else if(
        instance->protocol == NfcMagicProtocolUscuidUl ||
        instance->protocol == NfcMagicProtocolUscuidUlNotDetected) {
        instance->uscuid_ul_poller = uscuid_ul_poller_alloc(instance->nfc);
        // Pick the write transport from detection: direct (CUID/ATS) vs backdoor wakeup.
        // A not-detected tag has zeroed data (wakeup None) -> the direct engine.
        uscuid_ul_poller_set_wakeup(instance->uscuid_ul_poller, instance->uscuid_ul_data.wakeup);
        if(instance->uscuid_ul_password_set) {
            // Authenticate before writes so protected pages on a genuine/ATS tag are writable.
            uscuid_ul_poller_set_password(
                instance->uscuid_ul_poller, instance->uscuid_ul_password);
        }
        uscuid_ul_poller_start(
            instance->uscuid_ul_poller, nfc_magic_scene_write_uscuid_ul_poller_callback, instance);
    } else if(instance->protocol == NfcMagicProtocolIso15693) {
        instance->iso15693_poller = iso15693_poller_alloc(instance->nfc);
        if(instance->iso15693_is_wipe_mode) {
            // Zero every data block on the card (no source file).
            iso15693_poller_start_wipe(
                instance->iso15693_poller,
                nfc_magic_scene_write_iso15693_poller_callback,
                instance);
        } else {
            // Clone the loaded ISO15693 image (UID + writable blocks) onto the magic card.
            const Iso15693_3Data* source =
                nfc_device_get_data(instance->source_dev, NfcProtocolIso15693_3);
            iso15693_poller_start_clone(
                instance->iso15693_poller,
                source,
                nfc_magic_scene_write_iso15693_poller_callback,
                instance);
        }
    } else {
        instance->gen4_poller = gen4_poller_alloc(instance->nfc);
        gen4_poller_set_password(instance->gen4_poller, instance->gen4_password);
        gen4_poller_start(
            instance->gen4_poller, nfc_magic_scene_write_gen4_poller_callback, instance);
    }
}

bool nfc_magic_scene_write_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcMagicCustomEventCardDetected) {
            scene_manager_set_scene_state(
                instance->scene_manager, NfcMagicSceneWrite, NfcMagicSceneWriteStateCardFound);
            nfc_magic_scene_write_setup_view(instance);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventCardLost) {
            if(instance->protocol == NfcMagicProtocolIso15693) {
                // ISO15693 clone treats card-lost as terminal (not a resumable search).
                scene_manager_set_scene_state(
                    instance->scene_manager,
                    NfcMagicSceneIso15693WriteFail,
                    NfcMagicIso15693WriteFailReasonCardLost);
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneIso15693WriteFail);
            } else {
                scene_manager_set_scene_state(
                    instance->scene_manager,
                    NfcMagicSceneWrite,
                    NfcMagicSceneWriteStateCardSearch);
                nfc_magic_scene_write_setup_view(instance);
            }
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerProgress) {
            // Live "Writing X/N" while the USCUID-UL poller advances page by page.
            snprintf(
                instance->text_store,
                sizeof(instance->text_store),
                "%s\n%u / %u",
                instance->uscuid_ul_is_wipe_mode ? "Wiping" : "Writing",
                instance->write_progress_current,
                instance->write_progress_total);
            popup_set_header(
                instance->popup, instance->text_store, 52, 32, AlignLeft, AlignCenter);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerSuccess) {
            // An ISO15693 clone that succeeded but left the card advertising more blocks than it
            // physically holds (empty over-capacity, no data lost) gets a distinct "clone complete,
            // with a note" screen instead of the plain Success.
            if(instance->protocol == NfcMagicProtocolIso15693 &&
               instance->iso15693_clone_over_capacity > 0) {
                scene_manager_set_scene_state(
                    instance->scene_manager,
                    NfcMagicSceneIso15693WriteFail,
                    NfcMagicIso15693WriteFailReasonOverCapacity);
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneIso15693WriteFail);
            } else {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneSuccess);
            }
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerPartial) {
            // Gen2/Classic clone uses the shared per-block partial screen; USCUID-UL and ISO15693 have
            // their own.
            if(instance->protocol == NfcMagicProtocolGen2 ||
               instance->protocol == NfcMagicProtocolClassic) {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneGen2WipePartial);
            } else if(instance->protocol == NfcMagicProtocolIso15693) {
                scene_manager_set_scene_state(
                    instance->scene_manager,
                    NfcMagicSceneIso15693WriteFail,
                    NfcMagicIso15693WriteFailReasonPartial);
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneIso15693WriteFail);
            } else {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneUscuidUlPartial);
            }
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerAuthFail) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneUscuidUlAuthFail);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerFail) {
            if(instance->protocol == NfcMagicProtocolIso15693) {
                scene_manager_set_scene_state(
                    instance->scene_manager,
                    NfcMagicSceneIso15693WriteFail,
                    NfcMagicIso15693WriteFailReasonNotMagic);
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneIso15693WriteFail);
            } else {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWriteFail);
            }
            consumed = true;
        }
    }

    return consumed;
}

void nfc_magic_scene_write_on_exit(void* context) {
    NfcMagicApp* instance = context;

    if(instance->protocol == NfcMagicProtocolGen1) {
        gen1a_poller_stop(instance->gen1a_poller);
        gen1a_poller_free(instance->gen1a_poller);
    } else if(
        instance->protocol == NfcMagicProtocolGen2 ||
        instance->protocol == NfcMagicProtocolClassic) {
        gen2_poller_stop(instance->gen2_poller);
        gen2_poller_free(instance->gen2_poller);
    } else if(instance->protocol == NfcMagicProtocolGen4) {
        gen4_poller_stop(instance->gen4_poller);
        gen4_poller_free(instance->gen4_poller);
    } else if(
        instance->protocol == NfcMagicProtocolUscuidUl ||
        instance->protocol == NfcMagicProtocolUscuidUlNotDetected) {
        uscuid_ul_poller_stop(instance->uscuid_ul_poller);
        uscuid_ul_poller_free(instance->uscuid_ul_poller);
    } else if(instance->protocol == NfcMagicProtocolIso15693) {
        iso15693_poller_stop(instance->iso15693_poller);
        iso15693_poller_free(instance->iso15693_poller);
        instance->iso15693_poller = NULL;
    }
    scene_manager_set_scene_state(
        instance->scene_manager, NfcMagicSceneWrite, NfcMagicSceneWriteStateCardSearch);
    // Clear view
    popup_reset(instance->popup);

    nfc_magic_app_blink_stop(instance);
}
