#include "view_umi_test.h"
#include "../helpers/debug_log.h"
#include "../helpers/uhf_notifications.h"

static const char* umi_test_stage_text(UHFUmiTestStage stage) {
    switch(stage) {
    case UHFUmiTestStagePoll:
        return "Find tag";
    case UHFUmiTestStageSelect:
        return "Select tag";
    case UHFUmiTestStageReadUser:
        return "Read User bank";
    case UHFUmiTestStageWriteTemp:
        return "Write test word";
    case UHFUmiTestStageReadPc:
        return "Read test PC";
    case UHFUmiTestStageRestore:
        return "Restore User word";
    case UHFUmiTestStageVerifyRestore:
        return "Verify restore";
    case UHFUmiTestStageFinalPc:
        return "Read final PC";
    default:
        return "Unknown";
    }
}

static void umi_test_copy_worker_result(UHFReaderApp* App, UHFReaderUmiTestModel* Model) {
    UHFWorker* worker = App->YRM100XWorker;

    Model->outcome = worker->UmiTestOutcome;
    Model->failure_stage = worker->UmiTestFailureStage;
    Model->pc_before = worker->UmiTestPcBefore;
    Model->pc_during = worker->UmiTestPcDuring;
    Model->pc_after = worker->UmiTestPcAfter;
    memcpy(Model->user_before, worker->UmiTestUserBefore, 2);
    memcpy(Model->user_during, worker->UmiTestUserDuring, 2);
    memcpy(Model->user_after, worker->UmiTestUserAfter, 2);
    Model->user_bytes = worker->UmiTestUserBytes;
    Model->best_word = worker->UmiTestBestWord;
    Model->best_value = worker->UmiTestBestValue;
    Model->best_mask = worker->UmiTestBestMask;
    Model->pc3000_bits = worker->UmiTestPc3000Bits;
    Model->pc3400_bits = worker->UmiTestPc3400Bits;
    memcpy(Model->model_id, worker->UmiTestModelId, sizeof(Model->model_id));
    Model->model_valid = worker->UmiTestModelValid;
    Model->current_word = worker->UmiTestCurrentWord;
    Model->current_value = worker->UmiTestCurrentValue;
    Model->candidates_tested = worker->UmiTestCandidatesTested;
    Model->position_tests = worker->UmiTestPositionTests;
    Model->bit_tests = worker->UmiTestBitTests;
    Model->best_found = worker->UmiTestBestFound;
    Model->best_single_bit = worker->UmiTestBestSingleBit;
    Model->temporary_write_done = worker->UmiTestTemporaryWriteDone;
    Model->restore_verified = worker->UmiTestRestoreVerified;
    Model->last_status = worker->UmiTestLastStatus;
}

static bool umi_test_save_marker(UHFReaderApp* App) {
    UHFWorker* worker = App->YRM100XWorker;
    if(!worker || worker->UmiTestOutcome != UHFUmiTestOutcomeAffected ||
       !worker->UmiTestBestFound || !worker->UmiTestRestoreVerified ||
       !worker->UmiTestModelValid || worker->UmiTestBestMask == 0U) {
        return false;
    }

    UHFUmiMarkerEntry entry = {
        .word = worker->UmiTestBestWord,
        .mask = worker->UmiTestBestMask,
        .pc3000_bits = worker->UmiTestPc3000Bits,
        .pc3400_bits = worker->UmiTestPc3400Bits,
    };
    memcpy(entry.model_id, worker->UmiTestModelId, sizeof(entry.model_id));

    UHFUmiMarkerRegistry previous_registry = App->UmiMarkerRegistry;
    bool updated = uhf_umi_marker_registry_upsert(&App->UmiMarkerRegistry, &entry);
    bool saved = updated && uhf_umi_marker_registry_save(App->TagStorage, &App->UmiMarkerRegistry);
    if(!saved) App->UmiMarkerRegistry = previous_registry;

    UHF_I(
        "UMIDB",
        "SAVE model=%02X%02X%02X%02X W%u mask=%04X P3000=%04X P3400=%04X "
        "updated=%u saved=%u entries=%u",
        entry.model_id[0],
        entry.model_id[1],
        entry.model_id[2],
        entry.model_id[3],
        (unsigned int)entry.word,
        (unsigned int)entry.mask,
        (unsigned int)entry.pc3000_bits,
        (unsigned int)entry.pc3400_bits,
        updated ? 1U : 0U,
        saved ? 1U : 0U,
        (unsigned int)App->UmiMarkerRegistry.count);
    uhf_debug_flush();
    return saved;
}

uint32_t uhf_reader_navigation_umi_test_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewSubmenu;
}

void uhf_umi_test_worker_callback(UHFWorkerEvent event, void* context) {
    UHFReaderApp* App = context;

    if(event == UHFWorkerEventAborted) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        return;
    }

    notification_message(App->Notifications, &uhf_sequence_blink_stop);

    if(event == UHFWorkerEventSuccess) {
        uhf_notify_success(App);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventUmiTestDone);
    } else {
        uhf_notify_error(App);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventUmiTestFail);
    }
}

void uhf_reader_view_umi_test_draw_callback(Canvas* canvas, void* model) {
    UHFReaderUmiTestModel* Model = model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Test UMI Auto");

    canvas_set_font(canvas, FontSecondary);

    if(Model->phase == UmiTestPhaseReady) {
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, "WARNING: WRITES TAG");
        canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignTop, "Changes User bank data");
        canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignTop, "Backup: restore may fail");
        elements_button_center(canvas, "Start");
        return;
    }

    if(Model->phase == UmiTestPhaseRunning) {
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "Scanning User memory...");
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, "Do not remove tag");
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, "Restore after every test");
        return;
    }

    if(Model->phase == UmiTestPhaseResult) {
        char line[36];
        canvas_set_font(canvas, FontKeyboard);

        if(Model->outcome == UHFUmiTestOutcomeAlreadySet) {
            snprintf(line, sizeof(line), "PC %04X  UMI already=1", Model->pc_before);
            canvas_draw_str(canvas, 2, 18, line);

            snprintf(
                line,
                sizeof(line),
                "User %uB / %u words",
                (unsigned int)Model->user_bytes,
                (unsigned int)(Model->user_bytes / 2U));
            canvas_draw_str(canvas, 2, 29, line);

            canvas_draw_str(canvas, 2, 40, "No marker needed");
            canvas_draw_str(canvas, 2, 51, "No User data changed");
            elements_button_center(canvas, "Again");
            return;
        }

        if(Model->outcome == UHFUmiTestOutcomeAffected && Model->best_found) {
            snprintf(
                line,
                sizeof(line),
                "User %uB  best W%u=%04X",
                (unsigned int)Model->user_bytes,
                (unsigned int)Model->best_word,
                (unsigned int)Model->best_value);
            canvas_draw_str(canvas, 2, 16, line);

            snprintf(
                line,
                sizeof(line),
                "PC %04X>%04X>%04X",
                Model->pc_before,
                Model->pc_during,
                Model->pc_after);
            canvas_draw_str(canvas, 2, 27, line);

            snprintf(
                line,
                sizeof(line),
                "%s  tests=%u",
                Model->best_single_bit ? "1-bit marker" : "16-bit marker",
                (unsigned int)Model->candidates_tested);
            canvas_draw_str(canvas, 2, 38, line);

            snprintf(
                line,
                sizeof(line),
                "U%u %02X%02X>%02X%02X",
                (unsigned int)Model->best_word,
                Model->user_before[0],
                Model->user_before[1],
                Model->user_during[0],
                Model->user_during[1]);
            canvas_draw_str(canvas, 2, 49, line);

            canvas_draw_str(
                canvas,
                2,
                59,
                Model->marker_saved ?
                    "Restore OK / SAVED" :
                    (Model->restore_verified ? "Restore OK / not saved" : "Restore: ?"));
            return;
        }

        snprintf(line, sizeof(line), "No UMI=1 marker found");
        canvas_draw_str(canvas, 2, 20, line);

        snprintf(
            line,
            sizeof(line),
            "User %uB / %u words",
            (unsigned int)Model->user_bytes,
            (unsigned int)(Model->user_bytes / 2U));
        canvas_draw_str(canvas, 2, 32, line);

        snprintf(
            line,
            sizeof(line),
            "Tests %u  PC %04X",
            (unsigned int)Model->candidates_tested,
            Model->pc_after);
        canvas_draw_str(canvas, 2, 44, line);

        canvas_draw_str(
            canvas, 2, 55, Model->restore_verified ? "Restore: VERIFIED" : "Restore: ?");
        return;
    }

    if(Model->phase == UmiTestPhaseRestoreFailed) {
        char line[32];

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 13, AlignCenter, AlignTop, "RESTORE FAILED!");

        canvas_set_font(canvas, FontKeyboard);
        snprintf(line, sizeof(line), "Check User word %u", (unsigned int)Model->current_word);
        canvas_draw_str(canvas, 8, 31, line);

        snprintf(line, sizeof(line), "test value=%04X", (unsigned int)Model->current_value);
        canvas_draw_str(canvas, 8, 42, line);

        canvas_draw_str(canvas, 8, 53, "Do not retry blindly");
        return;
    }

    if(Model->phase == UmiTestPhaseFailed) {
        char line[36];
        canvas_set_font(canvas, FontKeyboard);

        snprintf(line, sizeof(line), "Failed: %s", umi_test_stage_text(Model->failure_stage));
        canvas_draw_str(canvas, 2, 18, line);

        snprintf(
            line,
            sizeof(line),
            "At W%u=%04X status=%d",
            (unsigned int)Model->current_word,
            (unsigned int)Model->current_value,
            (int)Model->last_status);
        canvas_draw_str(canvas, 2, 30, line);

        snprintf(
            line,
            sizeof(line),
            "Tests=%u User=%uB",
            (unsigned int)Model->candidates_tested,
            (unsigned int)Model->user_bytes);
        canvas_draw_str(canvas, 2, 42, line);

        if(Model->temporary_write_done && Model->restore_verified) {
            canvas_draw_str(canvas, 2, 54, "User restore: VERIFIED");
        } else if(!Model->temporary_write_done) {
            canvas_draw_str(canvas, 2, 54, "No temp data left");
        } else {
            canvas_draw_str(canvas, 2, 54, "CHECK RESTORE LOG");
        }
    }
}

static void umi_test_start(UHFReaderApp* App) {
    uhf_worker_stop(App->YRM100XWorker);

    with_view_model(
        App->ViewUmiTest,
        UHFReaderUmiTestModel * Model,
        {
            memset(Model, 0, sizeof(*Model));
            Model->phase = UmiTestPhaseRunning;
        },
        true);

    UHF_I("UMI", "UI AUTO START DefaultAP=0x%08lX", (unsigned long)App->YRM100XWorker->DefaultAP);
    uhf_debug_flush();

    notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);

    uhf_worker_start(App->YRM100XWorker, UHFWorkerStateUmiTest, uhf_umi_test_worker_callback, App);
}

bool uhf_reader_view_umi_test_input_callback(InputEvent* event, void* context) {
    UHFReaderApp* App = context;
    if(event->type != InputTypeShort) return false;

    UmiTestPhase phase;
    with_view_model(
        App->ViewUmiTest, UHFReaderUmiTestModel * Model, { phase = Model->phase; }, false);

    if(event->key == InputKeyOk && phase == UmiTestPhaseReady) {
        umi_test_start(App);
        return true;
    }

    if(event->key == InputKeyOk && phase != UmiTestPhaseRunning) {
        /* Require the warning screen before every repeated destructive test. */
        with_view_model(
            App->ViewUmiTest,
            UHFReaderUmiTestModel * Model,
            { Model->phase = UmiTestPhaseReady; },
            true);
        return true;
    }

    return false;
}

bool uhf_reader_view_umi_test_custom_event_callback(uint32_t event, void* context) {
    UHFReaderApp* App = context;

    if(event != UHFCustomEventUmiTestDone && event != UHFCustomEventUmiTestFail) {
        return false;
    }

    const bool marker_saved = event == UHFCustomEventUmiTestDone && umi_test_save_marker(App);

    with_view_model(
        App->ViewUmiTest,
        UHFReaderUmiTestModel * Model,
        {
            umi_test_copy_worker_result(App, Model);
            Model->marker_saved = marker_saved;

            if(App->YRM100XWorker->UmiTestOutcome == UHFUmiTestOutcomeRestoreFailed) {
                Model->phase = UmiTestPhaseRestoreFailed;
            } else if(event == UHFCustomEventUmiTestDone) {
                Model->phase = UmiTestPhaseResult;
            } else {
                Model->phase = UmiTestPhaseFailed;
            }
        },
        true);

    return true;
}

void uhf_reader_view_umi_test_enter_callback(void* context) {
    UHFReaderApp* App = context;

    uhf_worker_stop(App->YRM100XWorker);

    with_view_model(
        App->ViewUmiTest,
        UHFReaderUmiTestModel * Model,
        {
            memset(Model, 0, sizeof(*Model));
            Model->phase = UmiTestPhaseReady;
        },
        true);
}

void uhf_reader_view_umi_test_exit_callback(void* context) {
    UHFReaderApp* App = context;

    /*
     * If Back is pressed during the temporary-write window, worker_stop waits
     * for the worker. The worker ignores Stop only for its mandatory restore
     * path, so this callback does not return until restoration was attempted.
     */
    uhf_worker_stop(App->YRM100XWorker);
    notification_message(App->Notifications, &uhf_sequence_blink_stop);
}

void view_umi_test_alloc(UHFReaderApp* App) {
    App->ViewUmiTest = view_alloc();
    view_set_context(App->ViewUmiTest, App);
    view_allocate_model(App->ViewUmiTest, ViewModelTypeLocking, sizeof(UHFReaderUmiTestModel));

    view_set_draw_callback(App->ViewUmiTest, uhf_reader_view_umi_test_draw_callback);
    view_set_input_callback(App->ViewUmiTest, uhf_reader_view_umi_test_input_callback);
    view_set_custom_callback(App->ViewUmiTest, uhf_reader_view_umi_test_custom_event_callback);
    view_set_enter_callback(App->ViewUmiTest, uhf_reader_view_umi_test_enter_callback);
    view_set_exit_callback(App->ViewUmiTest, uhf_reader_view_umi_test_exit_callback);
    view_set_previous_callback(App->ViewUmiTest, uhf_reader_navigation_umi_test_callback);

    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewUmiTest, App->ViewUmiTest);
}

void view_umi_test_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewUmiTest);
    view_free(App->ViewUmiTest);
    App->ViewUmiTest = NULL;
}
