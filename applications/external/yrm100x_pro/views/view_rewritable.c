#include "view_rewritable.h"

static const char* rewritable_status_text(UHFRewriteBankStatus status) {
    switch(status) {
    case UHFRewriteBankStatusWritable:
        return "YES";
    case UHFRewriteBankStatusReadOnly:
        return "NO/LOCK";
    case UHFRewriteBankStatusProtected:
        return "AP PROTECT";
    case UHFRewriteBankStatusUnavailable:
        return "N/A";
    case UHFRewriteBankStatusRestoreFailed:
        return "RESTORE!";
    case UHFRewriteBankStatusError:
        return "RF ERROR";
    case UHFRewriteBankStatusUnknown:
    default:
        return "?";
    }
}

static uint32_t rewritable_back_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewBankTools;
}

static void rewritable_copy_result(UHFReaderApp* App, UHFReaderRewritableModel* Model) {
    Model->epc_status = App->YRM100XWorker->RewriteEpcStatus;
    Model->tid_status = App->YRM100XWorker->RewriteTidStatus;
    Model->user_status = App->YRM100XWorker->RewriteUserStatus;
    Model->reserved_status = App->YRM100XWorker->RewriteReservedStatus;
}

static bool rewritable_restore_failed(const UHFWorker* worker) {
    return worker->RewriteEpcStatus == UHFRewriteBankStatusRestoreFailed ||
           worker->RewriteTidStatus == UHFRewriteBankStatusRestoreFailed ||
           worker->RewriteUserStatus == UHFRewriteBankStatusRestoreFailed ||
           worker->RewriteReservedStatus == UHFRewriteBankStatusRestoreFailed;
}

static void rewritable_draw(Canvas* canvas, void* model) {
    UHFReaderRewritableModel* Model = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "TAG Rewritable");
    canvas_set_font(canvas, FontSecondary);

    if(Model->phase == RewritablePhaseRunning) {
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Testing four banks...");
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "Do not remove the tag");
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignTop, "Back = safe stop");
        return;
    }

    if(Model->phase == RewritablePhaseFailed) {
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Could not test tag");
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, "Check reader / RF / AP");
        elements_button_center(canvas, "Again");
        return;
    }

    char line[28];
    canvas_set_font(canvas, FontKeyboard);
    snprintf(line, sizeof(line), "EPC: %s", rewritable_status_text(Model->epc_status));
    canvas_draw_str(canvas, 4, 18, line);
    snprintf(line, sizeof(line), "TID: %s", rewritable_status_text(Model->tid_status));
    canvas_draw_str(canvas, 4, 29, line);
    snprintf(line, sizeof(line), "User: %s", rewritable_status_text(Model->user_status));
    canvas_draw_str(canvas, 4, 40, line);
    snprintf(line, sizeof(line), "Reserved: %s", rewritable_status_text(Model->reserved_status));
    canvas_draw_str(canvas, 4, 51, line);
    elements_button_center(canvas, "Again");
}

static void rewritable_worker_callback(UHFWorkerEvent event, void* context) {
    UHFReaderApp* App = context;
    notification_message(App->Notifications, &uhf_sequence_blink_stop);

    if(event == UHFWorkerEventAborted) return;
    view_dispatcher_send_custom_event(
        App->ViewDispatcher,
        event == UHFWorkerEventSuccess ? UHFCustomEventRewritableDone :
                                         UHFCustomEventRewritableFail);
}

static void rewritable_start(UHFReaderApp* App) {
    uhf_worker_stop(App->YRM100XWorker);
    App->YRM100XWorker->DefaultAP = bytes_to_uint32(App->ApTempBuffer, 4);

    with_view_model(
        App->ViewRewritable,
        UHFReaderRewritableModel * Model,
        {
            memset(Model, 0, sizeof(*Model));
            Model->phase = RewritablePhaseRunning;
        },
        true);

    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewRewritable);
    notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);
    uhf_worker_start(
        App->YRM100XWorker, UHFWorkerStateCheckRewritable, rewritable_worker_callback, App);
}

static void rewritable_confirm_callback(DialogExResult result, void* context) {
    UHFReaderApp* App = context;
    if(result == DialogExResultRight) {
        rewritable_start(App);
    } else {
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewBankTools);
    }
}

void uhf_reader_open_rewritable_confirm(UHFReaderApp* App) {
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewRewritableConfirm);
}

static bool rewritable_input(InputEvent* event, void* context) {
    UHFReaderApp* App = context;
    if(event->type != InputTypeShort) return false;

    RewritablePhase phase = RewritablePhaseRunning;
    with_view_model(
        App->ViewRewritable, UHFReaderRewritableModel * Model, { phase = Model->phase; }, false);

    if(event->key == InputKeyOk && phase != RewritablePhaseRunning) {
        uhf_reader_open_rewritable_confirm(App);
        return true;
    }
    return false;
}

static bool rewritable_custom_event(uint32_t event, void* context) {
    UHFReaderApp* App = context;
    if(event != UHFCustomEventRewritableDone && event != UHFCustomEventRewritableFail) {
        return false;
    }

    with_view_model(
        App->ViewRewritable,
        UHFReaderRewritableModel * Model,
        {
            rewritable_copy_result(App, Model);
            Model->phase = event == UHFCustomEventRewritableDone ? RewritablePhaseResult :
                                                                   RewritablePhaseFailed;
        },
        true);

    if(event == UHFCustomEventRewritableDone) {
        if(rewritable_restore_failed(App->YRM100XWorker)) {
            uhf_notify_error(App);
        } else {
            uhf_notify_success(App);
        }
    } else {
        uhf_notify_error(App);
    }
    return true;
}

static void rewritable_exit(void* context) {
    UHFReaderApp* App = context;
    uhf_worker_stop(App->YRM100XWorker);
    notification_message(App->Notifications, &uhf_sequence_blink_stop);
}

void view_rewritable_alloc(UHFReaderApp* App) {
    App->RewritableConfirmDialog = dialog_ex_alloc();
    dialog_ex_set_header(
        App->RewritableConfirmDialog, "WARNING!", 64, 8, AlignCenter, AlignCenter);
    dialog_ex_set_text(
        App->RewritableConfirmDialog,
        "TAG DATA MAY BE\nERASED!\nKeep ONE tag.",
        64,
        31,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(App->RewritableConfirmDialog, "Cancel");
    dialog_ex_set_right_button_text(App->RewritableConfirmDialog, "Start");
    dialog_ex_set_context(App->RewritableConfirmDialog, App);
    dialog_ex_set_result_callback(App->RewritableConfirmDialog, rewritable_confirm_callback);
    view_set_previous_callback(
        dialog_ex_get_view(App->RewritableConfirmDialog), rewritable_back_callback);
    view_dispatcher_add_view(
        App->ViewDispatcher,
        UHFReaderViewRewritableConfirm,
        dialog_ex_get_view(App->RewritableConfirmDialog));

    App->ViewRewritable = view_alloc();
    view_set_context(App->ViewRewritable, App);
    view_allocate_model(
        App->ViewRewritable, ViewModelTypeLocking, sizeof(UHFReaderRewritableModel));
    view_set_draw_callback(App->ViewRewritable, rewritable_draw);
    view_set_input_callback(App->ViewRewritable, rewritable_input);
    view_set_custom_callback(App->ViewRewritable, rewritable_custom_event);
    view_set_exit_callback(App->ViewRewritable, rewritable_exit);
    view_set_previous_callback(App->ViewRewritable, rewritable_back_callback);
    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewRewritable, App->ViewRewritable);
}

void view_rewritable_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewRewritableConfirm);
    dialog_ex_free(App->RewritableConfirmDialog);
    App->RewritableConfirmDialog = NULL;

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewRewritable);
    view_free(App->ViewRewritable);
    App->ViewRewritable = NULL;
}
