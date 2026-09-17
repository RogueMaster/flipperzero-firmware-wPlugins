#include "view_clone.h"
#include "../helpers/saved_epc_functions.h"
#include "view_read.h"

// ─── helpers ──────────────────────────────────────────────────────────────────

// Build a human-readable bank summary from a WriteMask bitmask.
// dst must be at least 28 bytes.
static void clone_mask_to_str(uint16_t mask, char* dst, size_t dst_size) {
    const char* epc = (mask & WRITE_EPC) ? "EPC " : "";
    const char* tid = (mask & WRITE_TID) ? "TID " : "";
    const char* user =
        (mask & UHF_CLONE_FORCE_PC3400) ?
            "User* " :
            ((mask & UHF_CLONE_FORCE_USER_ZERO) ? "User0 " : ((mask & WRITE_USER) ? "User " : ""));
    const char* rsvd = (mask & WRITE_RFU) ? "Rsvd" : "";
    snprintf(dst, dst_size, "%s%s%s%s", epc, tid, user, rsvd);
    // Trim trailing space
    size_t len = strlen(dst);
    if(len > 0 && dst[len - 1] == ' ') dst[len - 1] = '\0';
    if(dst[0] == '\0') snprintf(dst, dst_size, "None");
}

static bool clone_pc_mode_mismatch(uint16_t source_pc, uint16_t target_pc) {
    return (source_pc == UHF_CLONE_PC3000_SOURCE_PC && target_pc == UHF_CLONE_PC3400_SOURCE_PC) ||
           (source_pc == UHF_CLONE_PC3400_SOURCE_PC && target_pc == UHF_CLONE_PC3000_SOURCE_PC);
}

/*
 * If Clone rewrites MB2, remember the same physical target marker under the
 * newly emulated TID model as well. This keeps repeat clones working after the
 * original target model bytes have been replaced.
 */
static void clone_save_rewritten_tid_marker_alias(UHFReaderApp* App) {
    UHFWorker* worker = App->YRM100XWorker;
    if(!worker || !worker->CloneMarkerKnown || !(App->CloneMask & WRITE_TID) ||
       !App->CloneSourceTag || !App->CloneSourceTag->tid ||
       App->CloneSourceTag->tid->size < UHF_UMI_MODEL_ID_SIZE) {
        return;
    }

    UHFUmiMarkerEntry alias = {
        .word = worker->CloneMarkerWord,
        .mask = worker->CloneMarkerMask,
        .pc3000_bits = worker->CloneMarkerPc3000Bits,
        .pc3400_bits = worker->CloneMarkerPc3400Bits,
    };
    memcpy(alias.model_id, App->CloneSourceTag->tid->data, sizeof(alias.model_id));

    if(memcmp(alias.model_id, worker->CloneMarkerModelId, sizeof(alias.model_id)) == 0) return;

    UHFUmiMarkerRegistry previous_registry = App->UmiMarkerRegistry;
    bool updated = uhf_umi_marker_registry_upsert(&App->UmiMarkerRegistry, &alias);
    bool saved = updated && uhf_umi_marker_registry_save(App->TagStorage, &App->UmiMarkerRegistry);
    if(!saved) App->UmiMarkerRegistry = previous_registry;
    UHF_I(
        "UMIDB",
        "CLONE TID alias model=%02X%02X%02X%02X W%u mask=%04X saved=%u entries=%u",
        alias.model_id[0],
        alias.model_id[1],
        alias.model_id[2],
        alias.model_id[3],
        (unsigned int)alias.word,
        (unsigned int)alias.mask,
        saved ? 1U : 0U,
        (unsigned int)App->UmiMarkerRegistry.count);
    uhf_debug_flush();
}

// Stop any in-flight worker and (re)start the clone scan worker WITHOUT
// changing the view phase. Safe to call from the view-dispatcher (UI) thread.
static void start_clone_scan_worker(UHFReaderApp* App) {
    uhf_worker_stop(App->YRM100XWorker);
    uhf_worker_start(App->YRM100XWorker, UHFWorkerStateCloneScan, uhf_clone_worker_callback, App);
}

// Start (or restart) the clone scan worker and show the scanning screen.
static void start_clone_scan(UHFReaderApp* App) {
    App->YRM100XWorker->CloneModeConversionApproved = false;

    bool redraw = true;
    with_view_model(
        App->ViewClone,
        UHFReaderCloneModel * Model,
        {
            Model->phase = ClonePhaseScanning;
            Model->mode_conversion_applied = false;
            Model->source_pc = App->CloneSourceTag->epc->pc;
            Model->target_pc = 0U;
        },
        redraw);

    start_clone_scan_worker(App);
}

static void start_clone_write(UHFReaderApp* App, bool conversion_approved) {
    // Populate worker source (NewTag) from the deep-copied CloneSourceTag.
    UHFTag* src = App->CloneSourceTag;
    UHFTag* dest = App->YRM100XWorker->NewTag;
    uhf_tag_reset(dest);
    if(src->epc->size > 0) {
        uhf_tag_set_epc(dest, src->epc->data, src->epc->size);
        uhf_tag_set_epc_pc(dest, src->epc->pc);
        uhf_tag_set_epc_crc(dest, src->epc->crc);
    }
    if(src->tid->size > 0) uhf_tag_set_tid(dest, src->tid->data, src->tid->size);
    if(src->user->size > 0) uhf_tag_set_user(dest, src->user->data, src->user->size);
    if(src->reserved->size > 0) {
        uhf_tag_set_kill_pwd(dest, src->reserved->kill_password, 4);
        uhf_tag_set_access_pwd(dest, src->reserved->access_password, 4);
        dest->reserved->size = src->reserved->size;
    }

    App->YRM100XWorker->CloneMask = App->CloneMask;
    App->YRM100XWorker->CloneMaxAttempts = App->SettingCloneAttempts;
    App->YRM100XWorker->CloneAttemptCurrent = 1U;
    App->YRM100XWorker->CloneAttemptTotal = App->SettingCloneAttempts;
    App->YRM100XWorker->CloneModeConversionApproved = conversion_approved;
    App->YRM100XWorker->DefaultAP = 0; // fresh rewritable tags normally use zero AP
    App->YRM100XWorker->Targeted = false;

    UHF_I(
        "CLONE",
        "UI WRITE pressed mask=0x%04X attempts=%u conversion_approved=%u "
        "force_user_zero=%d pc3400=%d source PC=0x%04X EPC=%u TID=%u USER=%u RSV=%u",
        (unsigned int)App->CloneMask,
        (unsigned int)App->SettingCloneAttempts,
        conversion_approved ? 1U : 0U,
        (App->CloneMask & UHF_CLONE_FORCE_USER_ZERO) ? 1 : 0,
        (App->CloneMask & UHF_CLONE_FORCE_PC3400) ? 1 : 0,
        (unsigned int)src->epc->pc,
        (unsigned int)src->epc->size,
        (unsigned int)src->tid->size,
        (unsigned int)src->user->size,
        (unsigned int)src->reserved->size);
    uhf_debug_flush();

    bool redraw = true;
    with_view_model(
        App->ViewClone,
        UHFReaderCloneModel * Model,
        {
            Model->clone_attempt_current = 1U;
            Model->clone_attempt_total = App->SettingCloneAttempts;
            Model->mode_conversion_applied = false;
            Model->phase = ClonePhaseWriting;
        },
        redraw);

    notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);
    uhf_worker_start(App->YRM100XWorker, UHFWorkerStateCloneWrite, uhf_clone_worker_callback, App);
}

// ─── Worker callback ──────────────────────────────────────────────────────────

void uhf_clone_worker_callback(UHFWorkerEvent event, void* context) {
    UHFReaderApp* App = context;
    if(event == UHFWorkerEventCardDetected) {
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventCloneScanDone);
    } else if(event == UHFWorkerEventNoTagDetected) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventCloneScanTimeout);
    } else if(event == UHFWorkerEventSuccess) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        uhf_notify_success(App);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventCloneWriteDone);
    } else if(event == UHFWorkerEventAccessDenied) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        uhf_notify_error(App);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventCloneAccessDenied);
    } else if(event == UHFWorkerEventFail) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        uhf_notify_error(App);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventCloneWriteFail);
    } else if(event == UHFWorkerEventCloneProgress) {
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventCloneProgress);
    } else if(event == UHFWorkerEventCloneModeMismatch) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventCloneModeMismatch);
    } else if(event == UHFWorkerEventCloneMarkerUnknown) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        uhf_notify_error(App);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventCloneMarkerUnknown);
    }
    // UHFWorkerEventAborted: user pressed Back — no UI update needed
}

// ─── Draw callback ────────────────────────────────────────────────────────────

void uhf_reader_view_clone_draw_callback(Canvas* canvas, void* model) {
    UHFReaderCloneModel* Model = model;
    canvas_clear(canvas);

    switch(Model->phase) {
    case ClonePhaseScanning:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas,
            64,
            10,
            AlignCenter,
            AlignTop,
            Model->pc3400_mode ? "Clone PC3400" : "Clone Tag");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Place target tag...");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Scanning...");
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, "[Back] Cancel");
        break;

    case ClonePhaseConfirm: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Found Tag");
        canvas_set_font(canvas, FontSecondary);

        // EPC — truncate to 20 chars + "..." if longer
        char epc_display[24];
        size_t epc_len = strlen(Model->target_epc_str);
        if(epc_len > 20) {
            strncpy(epc_display, Model->target_epc_str, 20);
            epc_display[20] = '.';
            epc_display[21] = '.';
            epc_display[22] = '.';
            epc_display[23] = '\0';
        } else {
            strncpy(epc_display, Model->target_epc_str, sizeof(epc_display));
            epc_display[sizeof(epc_display) - 1] = '\0';
        }
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, epc_display);

        // Bank summary
        char bank_str[28] = "Write: ";
        clone_mask_to_str(Model->clone_mask, bank_str + 7, sizeof(bank_str) - 7);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, bank_str);

        // Hints
        elements_button_center(canvas, "Write");
        break;
    }

    case ClonePhaseModeMismatch: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Mode mismatch!");
        canvas_set_font(canvas, FontSecondary);

        char mode_text[24];
        snprintf(mode_text, sizeof(mode_text), "Dump PC: %04X", (unsigned int)Model->source_pc);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, mode_text);
        snprintf(mode_text, sizeof(mode_text), "Tag PC:  %04X", (unsigned int)Model->target_pc);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, mode_text);
        snprintf(
            mode_text, sizeof(mode_text), "Convert to PC%04X?", (unsigned int)Model->source_pc);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, mode_text);

        elements_button_left(canvas, "Cancel");
        elements_button_center(canvas, "Convert");
        break;
    }

    case ClonePhaseMarkerUnknown: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Unknown UMI tag");
        canvas_set_font(canvas, FontSecondary);

        char model_text[24];
        if(uhf_umi_model_id_valid(Model->marker_model_id)) {
            snprintf(
                model_text,
                sizeof(model_text),
                "TID: %02X%02X%02X%02X",
                Model->marker_model_id[0],
                Model->marker_model_id[1],
                Model->marker_model_id[2],
                Model->marker_model_id[3]);
        } else {
            snprintf(model_text, sizeof(model_text), "TID model unreadable");
        }
        canvas_draw_str_aligned(canvas, 64, 17, AlignCenter, AlignTop, model_text);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "Run Test UMI Auto");
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, "No data was written");
        elements_button_center(canvas, "Main");
        break;
    }

    case ClonePhaseWriting: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas,
            64,
            10,
            AlignCenter,
            AlignTop,
            Model->pc3400_mode ? "Clone PC3400" : "Clone Tag");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignTop, "Writing...");

        char attempt_text[20];
        snprintf(
            attempt_text,
            sizeof(attempt_text),
            "Attempt %u / %u",
            (unsigned int)Model->clone_attempt_current,
            (unsigned int)Model->clone_attempt_total);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignTop, attempt_text);
        break;
    }

    case ClonePhaseSuccess: {
        canvas_set_font(canvas, FontPrimary);
        const char* success_title = Model->pc3400_mode ? "PC3400 Cloned!" : "Cloned!";
        if(Model->mode_conversion_applied) {
            success_title = Model->source_pc == UHF_CLONE_PC3400_SOURCE_PC ? "PC3400 Cloned!" :
                                                                             "PC3000 Cloned!";
        }
        canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignTop, success_title);
        canvas_set_font(canvas, FontSecondary);

        bool reserved_selected = (Model->clone_mask & WRITE_RFU) != 0U;

        if(Model->mode_conversion_applied) {
            char mode_str[28];
            snprintf(
                mode_str,
                sizeof(mode_str),
                "Mode %04X -> %04X",
                (unsigned int)Model->target_pc,
                (unsigned int)Model->source_pc);
            canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignTop, mode_str);

            snprintf(
                mode_str,
                sizeof(mode_str),
                "PC:%04X CRC:%04X",
                (unsigned int)Model->final_pc,
                (unsigned int)Model->final_crc);
            canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, mode_str);
        } else if(Model->pc3400_mode) {
            char verify_str[28];
            snprintf(
                verify_str,
                sizeof(verify_str),
                "PC:%04X CRC:%04X",
                Model->final_pc,
                Model->final_crc);
            canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, verify_str);
            snprintf(
                verify_str,
                sizeof(verify_str),
                "User W%u=%04X",
                (unsigned int)Model->marker_word,
                (unsigned int)Model->marker_value);
            canvas_draw_str_aligned(canvas, 64, 33, AlignCenter, AlignTop, verify_str);

            if(reserved_selected) {
                char rsvd_str[30];
                snprintf(
                    rsvd_str,
                    sizeof(rsvd_str),
                    "Rsvd:%u/%uB skip:%u",
                    (unsigned int)Model->reserved_written_bytes,
                    (unsigned int)Model->reserved_source_bytes,
                    (unsigned int)Model->reserved_skipped_bytes);
                canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, rsvd_str);
            }
        } else {
            char line[30];

            if(reserved_selected) {
                snprintf(
                    line,
                    sizeof(line),
                    "Rsvd:%u/%uB skip:%u",
                    (unsigned int)Model->reserved_written_bytes,
                    (unsigned int)Model->reserved_source_bytes,
                    (unsigned int)Model->reserved_skipped_bytes);
                canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, line);

                snprintf(
                    line,
                    sizeof(line),
                    "Target Rsvd:%uB",
                    (unsigned int)Model->reserved_target_bytes);
                canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, line);
            } else {
                snprintf(line, sizeof(line), "Total: %u", (unsigned)Model->clone_count);
                canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, line);
            }
        }

        elements_button_center(canvas, "Scan Next");
        break;
    }

    case ClonePhaseTimeout:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "No Tag Found");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Timed out after 10s.");
        elements_button_center(canvas, "Scan Next");
        break;

    case ClonePhaseFailed: {
        canvas_set_font(canvas, FontPrimary);
        const bool mode_failure = Model->pc3400_mode || Model->mode_conversion_applied;
        const char* failure_title = "Write Failed";
        if(mode_failure) {
            failure_title = Model->source_pc == UHF_CLONE_PC3000_SOURCE_PC ? "PC3000 Failed" :
                                                                             "PC3400 Failed";
        }
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, failure_title);
        canvas_set_font(canvas, FontSecondary);

        if(mode_failure) {
            char final_str[24];
            snprintf(final_str, sizeof(final_str), "Final PC: %04X", Model->final_pc);
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, final_str);

            char attempt_str[28];
            snprintf(
                attempt_str,
                sizeof(attempt_str),
                "Try %u/%u  %s",
                (unsigned int)Model->clone_attempt_current,
                (unsigned int)Model->clone_attempt_total,
                Model->pc3400_marker_applied ? "ROLLBACK!" : "Rollback OK");
            canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignTop, attempt_str);
        } else {
            canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "Tag did not respond.");

            char attempt_str[24];
            snprintf(
                attempt_str,
                sizeof(attempt_str),
                "Attempts used: %u/%u",
                (unsigned int)Model->clone_attempt_current,
                (unsigned int)Model->clone_attempt_total);
            canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignTop, attempt_str);
        }

        elements_button_center(canvas, "Retry");
        break;
    }

    case ClonePhaseAccessDenied:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Access Denied");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Check AP / tag lock.");
        elements_button_center(canvas, "Retry");
        break;
    }
}

// ─── Input callback ───────────────────────────────────────────────────────────

bool uhf_reader_view_clone_input_callback(InputEvent* event, void* context) {
    UHFReaderApp* App = context;
    if(event->type != InputTypeShort) return false;

    ClonePhase phase;
    with_view_model(App->ViewClone, UHFReaderCloneModel * M, { phase = M->phase; }, false);

    if(phase == ClonePhaseModeMismatch && event->key == InputKeyLeft) {
        uint16_t source_pc = 0U;
        uint16_t target_pc = 0U;
        with_view_model(
            App->ViewClone,
            UHFReaderCloneModel * Model,
            {
                source_pc = Model->source_pc;
                target_pc = Model->target_pc;
            },
            false);
        UHF_I(
            "CLONEMODE",
            "USER CANCEL source=0x%04X target=0x%04X; no write",
            (unsigned int)source_pc,
            (unsigned int)target_pc);
        uhf_debug_flush();
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewCloneBanks);
        return true;
    }

    if(event->key == InputKeyOk) {
        if(phase == ClonePhaseConfirm) {
            start_clone_write(App, false);
            return true;
        }
        if(phase == ClonePhaseModeMismatch) {
            uint16_t source_pc = 0U;
            uint16_t target_pc = 0U;
            with_view_model(
                App->ViewClone,
                UHFReaderCloneModel * Model,
                {
                    source_pc = Model->source_pc;
                    target_pc = Model->target_pc;
                },
                false);
            UHF_I(
                "CLONEMODE",
                "USER APPROVE source=0x%04X target=0x%04X",
                (unsigned int)source_pc,
                (unsigned int)target_pc);
            uhf_debug_flush();
            start_clone_write(App, true);
            return true;
        }
        if(phase == ClonePhaseMarkerUnknown) {
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSubmenu);
            return true;
        }
        if(phase == ClonePhaseFailed || phase == ClonePhaseAccessDenied) {
            // Retry: go back to scanning
            start_clone_scan(App);
            return true;
        }
        if(phase == ClonePhaseSuccess || phase == ClonePhaseTimeout) {
            // Scan next: restart the scan loop for another target
            notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);
            start_clone_scan(App);
            return true;
        }
    }
    return false;
}

// ─── Custom event callback ────────────────────────────────────────────────────

bool uhf_reader_view_clone_custom_event_callback(uint32_t event, void* context) {
    UHFReaderApp* App = context;

    if(event == UHFCustomEventCloneScanDone) {
        // A tag was found by the scan worker. Capture its EPC for display.
        UHFTag* found = App->YRM100XWorker->uhf_tag_wrapper->uhf_tag;
        const uint16_t source_pc = App->CloneSourceTag->epc->pc;
        const uint16_t target_pc = found ? found->epc->pc : 0U;
        const bool mode_mismatch = (App->CloneMask & WRITE_EPC) != 0U &&
                                   clone_pc_mode_mismatch(source_pc, target_pc);
        char epc_hex[65] = {0};
        if(found && found->epc->size > 0) {
            char* hex = convert_to_hex_string(found->epc->data, found->epc->size);
            if(hex) {
                strncpy(epc_hex, hex, sizeof(epc_hex) - 1);
                free(hex);
            }
        } else {
            strncpy(epc_hex, "????????", sizeof(epc_hex) - 1);
        }

        UHF_I(
            "CLONEMODE",
            "SCAN source=0x%04X target=0x%04X mismatch=%u",
            (unsigned int)source_pc,
            (unsigned int)target_pc,
            mode_mismatch ? 1U : 0U);
        uhf_debug_flush();

        bool redraw = true;
        with_view_model(
            App->ViewClone,
            UHFReaderCloneModel * Model,
            {
                Model->phase = mode_mismatch ? ClonePhaseModeMismatch : ClonePhaseConfirm;
                strncpy(Model->target_epc_str, epc_hex, sizeof(Model->target_epc_str) - 1);
                Model->clone_mask = App->CloneMask;
                Model->pc3400_mode = App->ClonePc3400Mode;
                Model->source_pc = source_pc;
                Model->target_pc = target_pc;
                Model->mode_conversion_applied = false;
            },
            redraw);
        return true;
    }

    if(event == UHFCustomEventCloneModeMismatch) {
        const uint16_t source_pc = App->YRM100XWorker->CloneModeSourcePc;
        const uint16_t target_pc = App->YRM100XWorker->CloneModeTargetPc;
        UHF_W(
            "CLONEMODE",
            "WRITE GUARD requested UI confirmation source=0x%04X target=0x%04X",
            (unsigned int)source_pc,
            (unsigned int)target_pc);
        uhf_debug_flush();

        bool redraw = true;
        with_view_model(
            App->ViewClone,
            UHFReaderCloneModel * Model,
            {
                Model->source_pc = source_pc;
                Model->target_pc = target_pc;
                Model->mode_conversion_applied = false;
                Model->phase = ClonePhaseModeMismatch;
            },
            redraw);
        return true;
    }

    if(event == UHFCustomEventCloneMarkerUnknown) {
        bool redraw = true;
        with_view_model(
            App->ViewClone,
            UHFReaderCloneModel * Model,
            {
                Model->marker_known = false;
                memcpy(
                    Model->marker_model_id,
                    App->YRM100XWorker->CloneMarkerModelId,
                    sizeof(Model->marker_model_id));
                Model->phase = ClonePhaseMarkerUnknown;
            },
            redraw);
        return true;
    }

    if(event == UHFCustomEventCloneWriteDone) {
        clone_save_rewritten_tid_marker_alias(App);
        dolphin_deed(DolphinDeedNfcReadSuccess);
        uint8_t current = App->YRM100XWorker->CloneAttemptCurrent;
        uint8_t total = App->YRM100XWorker->CloneAttemptTotal;
        if(current == 0U) current = 1U;
        if(total == 0U) total = App->SettingCloneAttempts;

        bool redraw = true;
        with_view_model(
            App->ViewClone,
            UHFReaderCloneModel * Model,
            {
                Model->clone_count++;
                Model->pc3400_marker_applied = App->YRM100XWorker->ClonePc3400MarkerApplied;
                Model->pc3400_verified = App->YRM100XWorker->ClonePc3400Verified;
                Model->marker_known = App->YRM100XWorker->CloneMarkerKnown;
                memcpy(
                    Model->marker_model_id,
                    App->YRM100XWorker->CloneMarkerModelId,
                    sizeof(Model->marker_model_id));
                Model->marker_word = App->YRM100XWorker->CloneMarkerWord;
                Model->marker_value = App->YRM100XWorker->CloneMarkerAppliedValue;
                Model->final_pc = App->YRM100XWorker->ClonePc3400FinalPc;
                Model->final_crc = App->YRM100XWorker->ClonePc3400FinalCrc;
                Model->reserved_source_bytes = App->YRM100XWorker->CloneReservedSourceBytes;
                Model->reserved_target_bytes = App->YRM100XWorker->CloneReservedTargetBytes;
                Model->reserved_written_bytes = App->YRM100XWorker->CloneReservedWrittenBytes;
                Model->reserved_skipped_bytes = App->YRM100XWorker->CloneReservedSkippedBytes;
                Model->source_pc = App->YRM100XWorker->CloneModeSourcePc;
                Model->target_pc = App->YRM100XWorker->CloneModeTargetPc;
                Model->mode_conversion_applied = App->YRM100XWorker->CloneModeConversionApplied;
                Model->clone_attempt_current = current;
                Model->clone_attempt_total = total;
                Model->phase = ClonePhaseSuccess;
            },
            redraw);
        // Stay on the Success screen and wait for the user: [OK] scans the next
        // tag, [Back] exits. We do NOT auto-restart the scan loop.
        return true;
    }

    if(event == UHFCustomEventCloneProgress) {
        uint8_t current = App->YRM100XWorker->CloneAttemptCurrent;
        uint8_t total = App->YRM100XWorker->CloneAttemptTotal;
        if(current == 0U) current = 1U;
        if(total == 0U) total = App->SettingCloneAttempts;

        UHF_T(
            "CLONEUI",
            "PROGRESS display attempt=%u/%u",
            (unsigned int)current,
            (unsigned int)total);

        bool redraw = true;
        with_view_model(
            App->ViewClone,
            UHFReaderCloneModel * Model,
            {
                Model->clone_attempt_current = current;
                Model->clone_attempt_total = total;
            },
            redraw);
        return true;
    }

    if(event == UHFCustomEventCloneScanTimeout) {
        bool redraw = true;
        with_view_model(
            App->ViewClone,
            UHFReaderCloneModel * Model,
            { Model->phase = ClonePhaseTimeout; },
            redraw);
        return true;
    }

    if(event == UHFCustomEventCloneWriteFail) {
        uint8_t current = App->YRM100XWorker->CloneAttemptCurrent;
        uint8_t total = App->YRM100XWorker->CloneAttemptTotal;
        if(current == 0U) current = 1U;
        if(total == 0U) total = App->SettingCloneAttempts;

        bool redraw = true;
        with_view_model(
            App->ViewClone,
            UHFReaderCloneModel * Model,
            {
                Model->pc3400_marker_applied = App->YRM100XWorker->ClonePc3400MarkerApplied;
                Model->pc3400_verified = App->YRM100XWorker->ClonePc3400Verified;
                Model->marker_known = App->YRM100XWorker->CloneMarkerKnown;
                memcpy(
                    Model->marker_model_id,
                    App->YRM100XWorker->CloneMarkerModelId,
                    sizeof(Model->marker_model_id));
                Model->marker_word = App->YRM100XWorker->CloneMarkerWord;
                Model->marker_value = App->YRM100XWorker->CloneMarkerAppliedValue;
                Model->final_pc = App->YRM100XWorker->ClonePc3400FinalPc;
                Model->final_crc = App->YRM100XWorker->ClonePc3400FinalCrc;
                Model->reserved_source_bytes = App->YRM100XWorker->CloneReservedSourceBytes;
                Model->reserved_target_bytes = App->YRM100XWorker->CloneReservedTargetBytes;
                Model->reserved_written_bytes = App->YRM100XWorker->CloneReservedWrittenBytes;
                Model->reserved_skipped_bytes = App->YRM100XWorker->CloneReservedSkippedBytes;
                Model->source_pc = App->YRM100XWorker->CloneModeSourcePc;
                Model->target_pc = App->YRM100XWorker->CloneModeTargetPc;
                Model->mode_conversion_applied = App->YRM100XWorker->CloneModeConversionApplied;
                Model->clone_attempt_current = current;
                Model->clone_attempt_total = total;
                Model->phase = ClonePhaseFailed;
            },
            redraw);
        return true;
    }

    if(event == UHFCustomEventCloneAccessDenied) {
        bool redraw = true;
        with_view_model(
            App->ViewClone,
            UHFReaderCloneModel * Model,
            { Model->phase = ClonePhaseAccessDenied; },
            redraw);
        return true;
    }

    return false;
}

// ─── View lifecycle ───────────────────────────────────────────────────────────

void uhf_reader_view_clone_enter_callback(void* context) {
    UHFReaderApp* App = context;
    // Reset clone count on each fresh entry from the bank-selection screen
    bool redraw = true;
    with_view_model(
        App->ViewClone,
        UHFReaderCloneModel * Model,
        {
            Model->clone_count = 0;
            Model->clone_mask = App->CloneMask;
            Model->clone_attempt_current = 1U;
            Model->clone_attempt_total = App->SettingCloneAttempts;
            Model->source_pc = App->CloneSourceTag->epc->pc;
            Model->target_pc = 0U;
            Model->mode_conversion_applied = false;
            Model->pc3400_mode = App->ClonePc3400Mode;
            Model->pc3400_marker_applied = false;
            Model->pc3400_verified = false;
            Model->marker_known = false;
            memset(Model->marker_model_id, 0, sizeof(Model->marker_model_id));
            Model->marker_word = 0U;
            Model->marker_value = 0U;
            Model->final_pc = 0;
            Model->final_crc = 0;
            Model->reserved_source_bytes = 0;
            Model->reserved_target_bytes = 0;
            Model->reserved_written_bytes = 0;
            Model->reserved_skipped_bytes = 0;
        },
        redraw);
    notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);
    start_clone_scan(App);
}

void uhf_reader_view_clone_exit_callback(void* context) {
    UHFReaderApp* App = context;
    // Stop any in-flight worker
    uhf_worker_stop(App->YRM100XWorker);
    notification_message(App->Notifications, &uhf_sequence_blink_stop);
}

// ─── Navigation ──────────────────────────────────────────────────────────────

uint32_t uhf_reader_navigation_clone_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewCloneBanks;
}

// ─── Alloc / Free ─────────────────────────────────────────────────────────────

void view_clone_alloc(UHFReaderApp* App) {
    App->ViewClone = view_alloc();
    view_set_context(App->ViewClone, App);
    view_allocate_model(App->ViewClone, ViewModelTypeLocking, sizeof(UHFReaderCloneModel));
    view_set_draw_callback(App->ViewClone, uhf_reader_view_clone_draw_callback);
    view_set_input_callback(App->ViewClone, uhf_reader_view_clone_input_callback);
    view_set_custom_callback(App->ViewClone, uhf_reader_view_clone_custom_event_callback);
    view_set_enter_callback(App->ViewClone, uhf_reader_view_clone_enter_callback);
    view_set_exit_callback(App->ViewClone, uhf_reader_view_clone_exit_callback);
    view_set_previous_callback(App->ViewClone, uhf_reader_navigation_clone_callback);

    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewClone, App->ViewClone);
}

void view_clone_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewClone);
    view_free(App->ViewClone);
    App->ViewClone = NULL;
}
