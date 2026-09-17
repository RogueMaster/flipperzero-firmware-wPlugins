#include "view_bank_size.h"

static void bank_size_copy_worker_result(UHFReaderApp* App, UHFReaderBankSizeModel* Model) {
    UHFWorker* worker = App->YRM100XWorker;

    Model->epc_bytes = worker->BankSizeEpcBytes;
    Model->tid_bytes = worker->BankSizeTidBytes;
    Model->user_bytes = worker->BankSizeUserBytes;
    Model->reserved_bytes = worker->BankSizeReservedBytes;
    Model->epc_status = worker->BankSizeEpcStatus;
    Model->tid_status = worker->BankSizeTidStatus;
    Model->user_status = worker->BankSizeUserStatus;
    Model->reserved_status = worker->BankSizeReservedStatus;
    Model->pc = worker->BankSizePc;
    Model->crc = worker->BankSizeCrc;
}

static void bank_size_format_line(
    char* dst,
    size_t dst_size,
    const char* bank,
    uint16_t bytes,
    UHFSizeBankStatus status) {
    switch(status) {
    case UHFSizeBankStatusOk:
        snprintf(dst, dst_size, "%s: %u B", bank, (unsigned int)bytes);
        break;
    case UHFSizeBankStatusLocked:
        snprintf(dst, dst_size, "%s: LOCKED", bank);
        break;
    case UHFSizeBankStatusWrongPassword:
        snprintf(dst, dst_size, "%s: AP ERR", bank);
        break;
    case UHFSizeBankStatusError:
        if(bytes > 0U) {
            snprintf(dst, dst_size, "%s: >=%u B ?", bank, (unsigned int)bytes);
        } else {
            snprintf(dst, dst_size, "%s: RF ERR", bank);
        }
        break;
    case UHFSizeBankStatusUnknown:
    default:
        snprintf(dst, dst_size, "%s: ?", bank);
        break;
    }
}

uint32_t uhf_reader_navigation_bank_size_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewBankTools;
}

void uhf_bank_size_worker_callback(UHFWorkerEvent event, void* context) {
    UHFReaderApp* App = context;

    if(event == UHFWorkerEventAborted) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        return;
    }

    notification_message(App->Notifications, &uhf_sequence_blink_stop);

    if(event == UHFWorkerEventSuccess) {
        uhf_notify_success(App);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventBankSizeDone);
    } else {
        uhf_notify_error(App);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventBankSizeFail);
    }
}

void uhf_reader_view_bank_size_draw_callback(Canvas* canvas, void* model) {
    UHFReaderBankSizeModel* Model = model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Get Size Bank");
    canvas_set_font(canvas, FontSecondary);

    if(Model->phase == BankSizePhaseReady) {
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Place ONE tag in field");
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "RF-hardened byte probe");
        elements_button_center(canvas, "Scan");
        return;
    }

    if(Model->phase == BankSizePhaseRunning) {
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "Scanning bank sizes...");
        canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignTop, "Back = stop");
        return;
    }

    if(Model->phase == BankSizePhaseFailed) {
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, "Could not read tag");
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, "Check reader / RF");
        elements_button_center(canvas, "Again");
        return;
    }

    char line[28];
    canvas_set_font(canvas, FontKeyboard);

    bank_size_format_line(line, sizeof(line), "EPC", Model->epc_bytes, Model->epc_status);
    canvas_draw_str(canvas, 3, 17, line);

    bank_size_format_line(line, sizeof(line), "TID", Model->tid_bytes, Model->tid_status);
    canvas_draw_str(canvas, 3, 28, line);

    bank_size_format_line(line, sizeof(line), "User", Model->user_bytes, Model->user_status);
    canvas_draw_str(canvas, 3, 39, line);

    bank_size_format_line(
        line, sizeof(line), "Reserved", Model->reserved_bytes, Model->reserved_status);
    canvas_draw_str(canvas, 3, 50, line);

    snprintf(line, sizeof(line), "PC:%04X CRC:%04X", Model->pc, Model->crc);
    canvas_draw_str(canvas, 3, 61, line);
}

static void bank_size_start(UHFReaderApp* App) {
    uhf_worker_stop(App->YRM100XWorker);

    App->YRM100XWorker->DefaultAP = bytes_to_uint32(App->ApTempBuffer, 4);

    with_view_model(
        App->ViewBankSize,
        UHFReaderBankSizeModel * Model,
        {
            memset(Model, 0, sizeof(*Model));
            Model->phase = BankSizePhaseRunning;
        },
        true);

    UHF_I("SIZE", "UI START DefaultAP=0x%08lX", (unsigned long)App->YRM100XWorker->DefaultAP);
    uhf_debug_flush();

    notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);

    uhf_worker_start(
        App->YRM100XWorker, UHFWorkerStateGetBankSizes, uhf_bank_size_worker_callback, App);
}

bool uhf_reader_view_bank_size_input_callback(InputEvent* event, void* context) {
    UHFReaderApp* App = context;
    if(event->type != InputTypeShort) return false;

    BankSizePhase phase = BankSizePhaseReady;
    with_view_model(
        App->ViewBankSize, UHFReaderBankSizeModel * Model, { phase = Model->phase; }, false);

    if(event->key == InputKeyOk && phase != BankSizePhaseRunning) {
        bank_size_start(App);
        return true;
    }

    return false;
}

bool uhf_reader_view_bank_size_custom_event_callback(uint32_t event, void* context) {
    UHFReaderApp* App = context;

    if(event != UHFCustomEventBankSizeDone && event != UHFCustomEventBankSizeFail) {
        return false;
    }

    with_view_model(
        App->ViewBankSize,
        UHFReaderBankSizeModel * Model,
        {
            bank_size_copy_worker_result(App, Model);
            Model->phase = event == UHFCustomEventBankSizeDone ? BankSizePhaseResult :
                                                                 BankSizePhaseFailed;
        },
        true);

    return true;
}

void uhf_reader_view_bank_size_enter_callback(void* context) {
    UHFReaderApp* App = context;
    uhf_worker_stop(App->YRM100XWorker);

    with_view_model(
        App->ViewBankSize,
        UHFReaderBankSizeModel * Model,
        {
            memset(Model, 0, sizeof(*Model));
            Model->phase = BankSizePhaseReady;
        },
        true);
}

void uhf_reader_view_bank_size_exit_callback(void* context) {
    UHFReaderApp* App = context;
    uhf_worker_stop(App->YRM100XWorker);
    notification_message(App->Notifications, &uhf_sequence_blink_stop);
}

void view_bank_size_alloc(UHFReaderApp* App) {
    App->ViewBankSize = view_alloc();
    view_set_context(App->ViewBankSize, App);
    view_allocate_model(App->ViewBankSize, ViewModelTypeLocking, sizeof(UHFReaderBankSizeModel));

    view_set_draw_callback(App->ViewBankSize, uhf_reader_view_bank_size_draw_callback);
    view_set_input_callback(App->ViewBankSize, uhf_reader_view_bank_size_input_callback);
    view_set_custom_callback(App->ViewBankSize, uhf_reader_view_bank_size_custom_event_callback);
    view_set_enter_callback(App->ViewBankSize, uhf_reader_view_bank_size_enter_callback);
    view_set_exit_callback(App->ViewBankSize, uhf_reader_view_bank_size_exit_callback);
    view_set_previous_callback(App->ViewBankSize, uhf_reader_navigation_bank_size_callback);

    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewBankSize, App->ViewBankSize);
}

void view_bank_size_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewBankSize);
    view_free(App->ViewBankSize);
    App->ViewBankSize = NULL;
}
