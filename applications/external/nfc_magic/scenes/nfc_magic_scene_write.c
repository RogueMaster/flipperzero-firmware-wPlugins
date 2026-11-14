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

    if(event == Iso15693PollerEventWriteProgress) {
        // Same live counter the USCUID-UL clone drives, through the same app fields and event.
        iso15693_poller_get_result(instance->iso15693_poller, &instance->iso15693_result);
        instance->write_progress_current = instance->iso15693_result.blocks_done;
        instance->write_progress_total = instance->iso15693_result.blocks_total;
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerProgress);
    } else if(event == Iso15693PollerEventCardDetected) {
        // First activation: flip the popup off "Apply the same card" to "Writing" (mirrors the other
        // magic pollers, which all emit a card-detected event).
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventCardDetected);
    } else if(event == Iso15693PollerEventSuccess) {
        // Read the clone stats on success too: the Success handler below needs them to choose between
        // the bare popup and the result screen.
        iso15693_poller_get_result(instance->iso15693_poller, &instance->iso15693_result);
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerSuccess);
    } else if(event == Iso15693PollerEventPartial) {
        iso15693_poller_get_result(instance->iso15693_poller, &instance->iso15693_result);
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerPartial);
    } else if(event == Iso15693PollerEventCardLost) {
        // Read the result here too: a card lost DURING an opt-in gen1 run has still had the destructive
        // sequence written at blocks 56/57/62/63, and the card-lost screen is the only report the user
        // gets. Without this, gen1_attempted is stale and that damage goes unmentioned -- the same gap
        // the gen1-failure screen exists to close, on the path where the card left instead of refusing.
        // A clone lost after its pass reached 56/57 needs it too, for the note that its UID was never
        // read back (uid_recheck).
        iso15693_poller_get_result(instance->iso15693_poller, &instance->iso15693_result);
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcMagicCustomEventCardLost);
    } else if(event == Iso15693PollerEventNotGen2) {
        // gen2 left the UID unchanged (not a gen2 magic card). Offer the opt-in gen1 retry; nothing
        // has been written, so the card is untouched.
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventIso15693NotGen2);
    } else if(event == Iso15693PollerEventFail) {
        // Stash the result: every Fail carries its reason, and the fail handler picks the screen.
        iso15693_poller_get_result(instance->iso15693_poller, &instance->iso15693_result);
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, NfcMagicCustomEventWorkerFail);
    }
    // Any other event is not expected from the clone/wipe poller and is intentionally ignored.
}

// "Wiping" vs "Writing": the USCUID-UL and ISO15693 wipes each set their own flag. Resolved here
// once, because the header and the live progress line below both need it and must not diverge --
// a progress line testing only the USCUID-UL flag would call an ISO15693 wipe "Writing".
static bool nfc_magic_scene_write_is_wiping(NfcMagicApp* instance) {
    return instance->uscuid_ul_is_wipe_mode ||
           (instance->protocol == NfcMagicProtocolIso15693 &&
            instance->iso15693_mode == NfcMagicIso15693ModeWipe);
}

static void nfc_magic_scene_write_setup_view(NfcMagicApp* instance) {
    Popup* popup = instance->popup;
    popup_reset(popup);
    uint32_t state = scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneWrite);

    if(state == NfcMagicSceneWriteStateCardSearch) {
        // "the same card" means the one just scanned, which a hand-entered Write-UID never had.
        const bool write_uid = (instance->protocol == NfcMagicProtocolIso15693) &&
                               (instance->iso15693_mode == NfcMagicIso15693ModeWriteUid);
        popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
        popup_set_text(
            instance->popup,
            write_uid ? "Apply the\nmagic card\nto the back" : "Apply the\nsame card\nto the back",
            128,
            32,
            AlignRight,
            AlignCenter);
    } else {
        const bool is_wipe = nfc_magic_scene_write_is_wiping(instance);
        popup_set_icon(popup, 12, 23, &I_Loading_24);
        popup_set_header(
            popup,
            is_wipe ? "Wiping\nDon't move..." : "Writing\nDon't move...",
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
        // CONSUME the grant: it is for THIS run, not for the mode. Retry re-enters this scene
        // directly, so a grant left standing writes 56/57/62/63 on whatever card is then on the coil
        // -- and Retry does not re-identify the card. Clear it above the branches, not in them: the
        // wipe arm never reads it and would otherwise pass it on.
        const bool force_gen1 = instance->iso15693_force_gen1;
        instance->iso15693_force_gen1 = false;
        if(instance->iso15693_mode == NfcMagicIso15693ModeWriteUid) {
            // Write a hand-entered UID, no source image. gen2 first; the opt-in gen1 retry re-enters
            // this scene with the grant set, exactly as the clone does.
            if(force_gen1) {
                iso15693_poller_start_write_uid_gen1(
                    instance->iso15693_poller,
                    instance->iso15693_target_uid,
                    nfc_magic_scene_write_iso15693_poller_callback,
                    instance);
            } else {
                iso15693_poller_start_write_uid(
                    instance->iso15693_poller,
                    instance->iso15693_target_uid,
                    nfc_magic_scene_write_iso15693_poller_callback,
                    instance);
            }
        } else if(instance->iso15693_mode == NfcMagicIso15693ModeWipe) {
            // Zero every data block on the card (no source file).
            iso15693_poller_start_wipe(
                instance->iso15693_poller,
                nfc_magic_scene_write_iso15693_poller_callback,
                instance);
        } else {
            // Clone the loaded ISO15693 image onto the magic card. The gen2 attempt runs first;
            // if the user opted into the destructive gen1 retry on the "not gen2 magic" screen
            // (iso15693_force_gen1), run that instead.
            const Iso15693_3Data* source =
                nfc_device_get_data(instance->source_dev, NfcProtocolIso15693_3);
            if(force_gen1) {
                iso15693_poller_start_clone_gen1(
                    instance->iso15693_poller,
                    source,
                    nfc_magic_scene_write_iso15693_poller_callback,
                    instance);
            } else {
                iso15693_poller_start_clone(
                    instance->iso15693_poller,
                    source,
                    nfc_magic_scene_write_iso15693_poller_callback,
                    instance);
            }
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
                // ISO15693 treats card-lost as terminal (not a resumable search).
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
            // Live "Writing X/N" or "Wiping X/N" as the poller advances: USCUID-UL per page,
            // ISO15693 per block. Both feed the same fields and the same event.
            snprintf(
                instance->text_store,
                sizeof(instance->text_store),
                "%s\n%u / %u",
                nfc_magic_scene_write_is_wiping(instance) ? "Wiping" : "Writing",
                instance->write_progress_current,
                instance->write_progress_total);
            popup_set_header(
                instance->popup, instance->text_store, 52, 32, AlignLeft, AlignCenter);
            consumed = true;
        } else if(event.event == NfcMagicCustomEventWorkerSuccess) {
            // Deliberately not nfc_magic_scene_write_is_wiping(): that is true for a USCUID-UL wipe too.
            const bool iso15693 = (instance->protocol == NfcMagicProtocolIso15693);
            const bool iso15693_wipe = iso15693 &&
                                       (instance->iso15693_mode == NfcMagicIso15693ModeWipe);
            if(iso15693 && instance->iso15693_mode == NfcMagicIso15693ModeWriteUid) {
                // The poller verified the new UID reads back, so refresh the stored read result too --
                // otherwise re-entering Write UID without a fresh Info read would seed the byte editor
                // from the pre-write UID and look as though the write hadn't taken.
                memcpy(
                    instance->iso15693_data->uid,
                    instance->iso15693_target_uid,
                    ISO15693_3_UID_SIZE);
            }
            // Three ISO15693 successes carry information the bare "Success!" popup has nowhere to
            // put: a clone whose file was larger than the card (empty over-capacity, no data lost),
            // a clone that ends with a note -- the survey's three findings, or a gen1 card whose
            // four register addresses the file reached with nothing there, all listed at
            // NfcMagicIso15693WriteFailReasonCloneComplete -- and any wipe, whose sweep length is
            // measured, so a run that stopped short of the card's claim and one that covered it
            // would otherwise look the same. All three go to the result screen. The notes' test
            // below has five terms for four findings, because the card's reported block count and
            // IC reference are recorded separately. It has no mode term: both places that read it
            // test for a wipe first, and a wipe never sets these fields.
            const bool clone_notes = iso15693 && (instance->iso15693_result.residue_found ||
                                                  instance->iso15693_result.holds_more ||
                                                  instance->iso15693_result.memory_differs ||
                                                  instance->iso15693_result.ic_ref_differs ||
                                                  (instance->iso15693_result.used_gen1 &&
                                                   instance->iso15693_result.gen1_blocks_skipped));
            if(iso15693_wipe || (iso15693 && instance->iso15693_result.over_capacity > 0) ||
               clone_notes) {
                scene_manager_set_scene_state(
                    instance->scene_manager,
                    NfcMagicSceneIso15693WriteFail,
                    iso15693_wipe ? NfcMagicIso15693WriteFailReasonWipeComplete :
                    clone_notes   ? NfcMagicIso15693WriteFailReasonCloneComplete :
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
                // A wipe whose blocks cleared but whose UID moved gets its own screen, ahead of the
                // ordinary partial: the block counts are beside the point next to the card's identity
                // changing under an operation that never sends a UID command.
                // Order is a priority: a moved UID outranks a cut sweep, which outranks ordinary
                // block failures. All three are Partial to the poller; they differ in what the user
                // most needs told.
                // The order is also what decides Retry, which is worth saying here because the
                // predicate that grants it cannot: a wipe that was BOTH cut and moved the UID takes the
                // UID branch, and WipeUidChanged is deliberately not retryable -- on gen1 a re-run is
                // another pass over 56/57, the very blocks that moved it. The truncation note still
                // reaches Details there, so the cut is stated; only the button is withheld.
                NfcMagicIso15693WriteFailReason partial_reason;
                if(instance->iso15693_result.uid_changed) {
                    partial_reason = NfcMagicIso15693WriteFailReasonWipeUidChanged;
                } else if(
                    instance->iso15693_result.pass_truncated &&
                    instance->iso15693_mode == NfcMagicIso15693ModeWipe) {
                    // Mode-gated because a clone's data pass sets pass_truncated too, and
                    // WipeStopped's screen is wipe-specific down to its wording. A cut clone stays on
                    // the ordinary partial screen and is qualified there.
                    partial_reason = NfcMagicIso15693WriteFailReasonWipeStopped;
                } else {
                    partial_reason = NfcMagicIso15693WriteFailReasonPartial;
                }
                scene_manager_set_scene_state(
                    instance->scene_manager, NfcMagicSceneIso15693WriteFail, partial_reason);
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
                // Pick the reason: a Write UID with nothing to prove, a UID that moved somewhere
                // unasked-for, a spent gen1 attempt, a wipe that cleared nothing, an empty-source
                // clone (blocks_total == 0), or a clone whose UID took but whose every data block was
                // rejected. Order matters throughout -- an empty source would satisfy the all-rejected
                // test trivially, and the three UID/gen1 outcomes cut across every mode.
                //
                // NotMagic is only the defensive fallback. A card that simply isn't magic leaves the
                // UID unchanged, which is NotGen2 rather than Fail -- it reaches the gen1 opt-in
                // screen, and declining there returns to the menu. So nothing routed here is known to
                // be an ordinary tag.
                NfcMagicIso15693WriteFailReason reason;
                if(instance->iso15693_result.uid_unverifiable) {
                    // Write UID asked for the UID the card already has, so nothing was sent. Checked
                    // first: it is the one Fail where the card was never written to at all.
                    reason = NfcMagicIso15693WriteFailReasonUidUnverifiable;
                } else if(instance->iso15693_result.uid_unexpected) {
                    // The UID moved, just not to what was asked for. Ahead of every mode-specific
                    // reason below, because it is the one Fail that proves the card IS magic and
                    // "Not a magic tag" would be exactly backwards.
                    reason = NfcMagicIso15693WriteFailReasonUidUnexpected;
                } else if(
                    instance->iso15693_result.gen1_attempted &&
                    !instance->iso15693_result.used_gen1) {
                    // The opt-in gen1 sequence went out and the UID did NOT verify. Blocks 56/57/62/63
                    // are overwritten either way, so this outranks the mode-specific reasons too --
                    // the user needs to know what was spent, not just that it didn't work. The
                    // used_gen1 term matters: a gen1 clone whose UID DID take and then lost every data
                    // block is the "UID only" failure below, not a gen1 UID failure.
                    reason = NfcMagicIso15693WriteFailReasonGen1Failed;
                } else if(instance->iso15693_mode == NfcMagicIso15693ModeWriteUid) {
                    // A Write-UID has no source blocks, so blocks_total is 0 for an unrelated reason
                    // and must not fall through to the empty-source test below.
                    reason = NfcMagicIso15693WriteFailReasonNotMagic;
                } else if(instance->iso15693_mode == NfcMagicIso15693ModeWipe) {
                    reason = NfcMagicIso15693WriteFailReasonNothingWiped;
                } else if(instance->iso15693_result.blocks_total == 0) {
                    reason = NfcMagicIso15693WriteFailReasonEmptySource;
                } else if(
                    instance->iso15693_result.failed_count >=
                    instance->iso15693_result.blocks_total) {
                    reason = NfcMagicIso15693WriteFailReasonNothingCloned;
                } else {
                    reason = NfcMagicIso15693WriteFailReasonNotMagic;
                }
                scene_manager_set_scene_state(
                    instance->scene_manager, NfcMagicSceneIso15693WriteFail, reason);
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneIso15693WriteFail);
            } else {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWriteFail);
            }
            consumed = true;
        } else if(event.event == NfcMagicCustomEventIso15693NotGen2) {
            // gen2 left the UID unchanged -> offer the opt-in gen1 retry on a dedicated screen. Tell
            // it which flow it came from: a clone consents to a full data-block write, a Write-UID only
            // to the four gen1 registers.
            scene_manager_set_scene_state(
                instance->scene_manager,
                NfcMagicSceneIso15693Gen1Optin,
                (instance->iso15693_mode == NfcMagicIso15693ModeWriteUid) ?
                    NfcMagicIso15693Gen1OptinFromWriteUid :
                    NfcMagicIso15693Gen1OptinFromClone);
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneIso15693Gen1Optin);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // ISO15693 only: once a card has been found, Back is swallowed until the poller reports an
        // outcome.
        //
        // Back cannot abort a write: leaving this scene runs on_exit -> <proto>_poller_stop ->
        // furi_thread_join, which waits for the worker to finish whatever it is doing. Measured on
        // an ISO15693 wipe: the key-down arrives, the GUI thread sits in the join, the worker runs
        // the sweep to completion, and only then does the view change. So the write happens either
        // way; leaving would only discard the report.
        //
        // Worse for a clone, which has a NfcCommandReset between the UID write and the data pass. Back
        // landing in that gap leaves the card carrying a new UID and reprogrammed geometry with none of
        // the source's data -- the "UID only" state the nothing_cloned screen exists to warn about,
        // reached silently, and at the very start of the write, which is exactly when someone who has
        // changed their mind presses Back.
        //
        // NOT extended to the other four magic protocols, even though the same "Back discards rather
        // than aborts" argument applies to them, because for them it would be a trap. Swallowing Back
        // is only safe where the write is guaranteed to report an outcome.
        //
        // ISO15693 is: iso15693_poller_nfc_callback counts activation failures, against
        // ISO15693_POLLER_MAX_ACTIVATION_ERRORS in general and the shorter
        // ISO15693_POLLER_WIPE_VERIFY_ACTIVATIONS while a wipe's UID check is outstanding -- the first
        // reports CardLost, the second reports the wipe's own result. Either way an outcome arrives.
        //
        // gen1a and the USCUID-UL backdoor engine would also be safe -- they run on raw nfc_start, so
        // PollerReady is a poll-cycle tick and their handlers reach Fail or Partial with no card -- and
        // are excluded only for want of testing.
        //
        // The genuinely unsafe ones are gen2/Classic, USCUID-direct and gen4: all are driven by an
        // activation-Ready event and stop advancing once the card is gone, discarding the poller's
        // Error event so the state machine is never called again. Measured at 88 seconds with no
        // state-machine activity (#252), where Back is the only way off the popup, so swallowing it
        // there needs a reboot to recover (#253).
        //
        // The card-search phase is untouched: nothing has been written there, so Back still leaves. Any
        // terminal outcome releases the button.
        consumed =
            (instance->protocol == NfcMagicProtocolIso15693 &&
             scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneWrite) ==
                 NfcMagicSceneWriteStateCardFound);
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
