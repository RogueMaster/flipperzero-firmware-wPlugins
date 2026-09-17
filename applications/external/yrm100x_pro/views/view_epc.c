#include "view_epc.h"
#include "view_read.h"
#include "../helpers/saved_epc_functions.h"

// ─── Draw callback ────────────────────────────────────────────────────────────
void uhf_reader_view_epc_draw_callback(Canvas* canvas, void* model) {
    UHFRFIDTagModel* MyModel = (UHFRFIDTagModel*)model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    // "EPC" headline top-left, matching the memory screens style
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 11, "EPC");
    canvas_set_font(canvas, FontSecondary);
    // RSSI top-right
    char RssiStr[16];
    snprintf(RssiStr, sizeof(RssiStr), "RSSI:%ddBm", (int)MyModel->Rssi);
    canvas_draw_str_aligned(canvas, 128, 11, AlignRight, AlignBottom, RssiStr);

    // EPC value — wrap at 20 chars per line (120px fits 128px display)
    const char* EpcStr = furi_string_get_cstr(MyModel->Epc);
    size_t EpcLen = strlen(EpcStr);
    const size_t CharsPerLine = 20;

    char Line1[21];
    memset(Line1, 0, sizeof(Line1));
    size_t L1 = EpcLen < CharsPerLine ? EpcLen : CharsPerLine;
    memcpy(Line1, EpcStr, L1);
    canvas_draw_str(canvas, 0, 22, Line1);

    if(EpcLen > CharsPerLine) {
        char Line2[21];
        memset(Line2, 0, sizeof(Line2));
        size_t L2 = (EpcLen - CharsPerLine) < CharsPerLine ? (EpcLen - CharsPerLine) :
                                                             CharsPerLine;
        memcpy(Line2, EpcStr + CharsPerLine, L2);
        canvas_draw_str(canvas, 0, 33, Line2);
    }

    // CRC and PC on one row
    canvas_draw_str(canvas, 4, 44, "CRC:");
    canvas_draw_str(canvas, 28, 44, furi_string_get_cstr(MyModel->Crc));
    canvas_draw_str(canvas, 70, 44, "PC:");
    canvas_draw_str(canvas, 88, 44, furi_string_get_cstr(MyModel->Pc));

    // Bank browse ring: Left → User bank, Right → TID bank; center opens Actions.
    elements_button_left(canvas, "Usr");
    elements_button_center(canvas, "Action");
    elements_button_right(canvas, "TID");
}

// ─── Navigation callbacks ─────────────────────────────────────────────────────
uint32_t uhf_reader_navigation_exit_view_epc_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewRead;
}

uint32_t uhf_reader_navigation_banks_to_epc_dump_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewEpcDump;
}

// ─── Input handler ────────────────────────────────────────────────────────────
bool uhf_reader_view_epc_input_callback(InputEvent* event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    if(event->type != InputTypeShort) return false;

    // Center (OK): open the Tag Action menu (Update / Lock / Kill) for this
    // in-memory scanned tag. Mark the menu context as "live" so it shows the
    // unsaved action set and downstream views target this specific tag.
    if(event->key == InputKeyOk && App->NumberOfEpcsToRead > 0) {
        App->ActionContext = ActionFromLive;
        uhf_reader_ensure_tag_actions_view(App);
        uhf_reader_build_tag_action_menu(App);
        view_set_previous_callback(
            submenu_get_view(App->SubmenuTagActions),
            uhf_reader_navigation_banks_to_epc_dump_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewTagAction);
        return true;
    }

    // Right: browse forward to the TID bank screen (next node in the ring).
    if(event->key == InputKeyRight && App->NumberOfEpcsToRead > 0) {
        with_view_model(
            App->ViewBankMem,
            UHFRFIDTagModel * m,
            {
                m->CurrentBank = 0;
                m->VScrollLine = 0;
            },
            false);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewBankMem);
        return true;
    }

    // Left: browse backward to the User bank screen (previous node in the ring).
    if(event->key == InputKeyLeft && App->NumberOfEpcsToRead > 0) {
        with_view_model(
            App->ViewBankMem,
            UHFRFIDTagModel * m,
            {
                m->CurrentBank = 2;
                m->VScrollLine = 0;
            },
            false);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewBankMem);
        return true;
    }

    return false;
}

// ─── Custom event handler ─────────────────────────────────────────────────────
bool uhf_reader_view_epc_custom_event_callback(uint32_t event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    switch(event) {
    case UHFReaderEventIdRedrawScreen: {
        with_view_model(App->ViewEpc, UHFRFIDTagModel * _m, { UNUSED(_m); }, true);
        return true;
    }

    case UHFCustomEventDeepReadDone: {
        // Stop and free the 5-second timeout timer
        if(App->Timer) {
            furi_timer_stop(App->Timer);
            furi_timer_free(App->Timer);
            App->Timer = NULL;
        }
        uhf_worker_stop(App->YRM100XWorker);

        App->DeepReading = false;
        App->DeepReadDone = true;

        // Populate the Banks screen (ViewEpcInfo) with the freshly read data
        UHFTag* tag = App->YRM100XWorker->SelectedTag;

        char* TempEpc = convertToHexString(tag->epc->data, tag->epc->size);
        char* TempTid = convertToHexString(tag->tid->data, tag->tid->size);
        char* TempUser = convertToHexString(tag->user->data, tag->user->size);
        // Show the full Reserved bank exactly as read (variable length). Fall back to
        // the two 4-byte passwords only if the full buffer wasn't populated.
        char* TempRes;
        if(tag->reserved->size > 0) {
            TempRes = convertToHexString(tag->reserved->data, tag->reserved->size);
        } else {
            uint8_t reserved_buf[8];
            memcpy(reserved_buf, tag->reserved->kill_password, 4);
            memcpy(reserved_buf + 4, tag->reserved->access_password, 4);
            TempRes = convertToHexString(reserved_buf, 8);
        }
        char* TempCrc = uint16_to_hex_string(tag->epc->crc);
        char* TempPc = uint16_to_hex_string(tag->epc->pc);

        App->NumberOfTidsToRead = 1;
        App->NumberOfResToRead = 1;
        App->NumberOfMemToRead = 1;

        with_view_model(
            App->ViewEpcInfo,
            UHFRFIDTagModel * _model,
            {
                furi_string_set_str(_model->Epc, TempEpc);
                furi_string_set_str(_model->Tid, TempTid);
                furi_string_set_str(_model->User, TempUser);
                furi_string_set_str(_model->Reserved, TempRes);
                furi_string_set_str(_model->Crc, TempCrc);
                furi_string_set_str(_model->Pc, TempPc);
            },
            false);

        // Mirror the deep-read banks into the EPC dump model so an Up-key save
        // from either screen has the full TID/User/Reserved/CRC/PC available.
        with_view_model(
            App->ViewEpc,
            UHFRFIDTagModel * _model,
            {
                _model->IsDeepReading = false;
                _model->DeepReadDone = true;
                furi_string_set_str(_model->Tid, TempTid);
                furi_string_set_str(_model->User, TempUser);
                furi_string_set_str(_model->Reserved, TempRes);
                furi_string_set_str(_model->Crc, TempCrc);
                furi_string_set_str(_model->Pc, TempPc);
                _model->TidBankRead = true;
                _model->UserBankRead = true;
                _model->ResBankRead = true;
            },
            true);

        free(TempEpc);
        free(TempTid);
        free(TempUser);
        free(TempRes);
        free(TempCrc);
        free(TempPc);

        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        return true;
    }

    case UHFCustomEventDeepReadAborted: {
        // Stop and free the 5-second timeout timer
        if(App->Timer) {
            furi_timer_stop(App->Timer);
            furi_timer_free(App->Timer);
            App->Timer = NULL;
        }
        App->DeepReading = false;

        with_view_model(
            App->ViewEpc, UHFRFIDTagModel * _model, { _model->IsDeepReading = false; }, true);

        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        return true;
    }

    default:
        return false;
    }
}

// ─── Enter / Exit callbacks ───────────────────────────────────────────────────
void uhf_reader_view_epc_enter_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    // Sync deep-read state into the view model on every entry
    with_view_model(
        App->ViewEpc,
        UHFRFIDTagModel * model,
        {
            model->IsDeepReading = App->DeepReading;
            model->DeepReadDone = App->DeepReadDone;
        },
        true);
}

void uhf_reader_view_epc_exit_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    // Safety: if a deep-read is somehow in progress, clean up the timer
    if(App->Timer) {
        furi_timer_stop(App->Timer);
        furi_timer_free(App->Timer);
        App->Timer = NULL;
    }
}

static void uhf_tag_model_strings_free(UHFRFIDTagModel* model) {
    if(!model) return;

    if(model->Epc) {
        furi_string_free(model->Epc);
        model->Epc = NULL;
    }
    if(model->Tid) {
        furi_string_free(model->Tid);
        model->Tid = NULL;
    }
    if(model->User) {
        furi_string_free(model->User);
        model->User = NULL;
    }
    if(model->Reserved) {
        furi_string_free(model->Reserved);
        model->Reserved = NULL;
    }
    if(model->Crc) {
        furi_string_free(model->Crc);
        model->Crc = NULL;
    }
    if(model->Pc) {
        furi_string_free(model->Pc);
        model->Pc = NULL;
    }
}

// ─── Allocation / Free ────────────────────────────────────────────────────────
void view_epc_alloc(UHFReaderApp* App) {
    App->ViewEpc = view_alloc();
    view_set_draw_callback(App->ViewEpc, uhf_reader_view_epc_draw_callback);
    view_set_input_callback(App->ViewEpc, uhf_reader_view_epc_input_callback);
    view_set_custom_callback(App->ViewEpc, uhf_reader_view_epc_custom_event_callback);
    view_set_previous_callback(App->ViewEpc, uhf_reader_navigation_exit_view_epc_callback);
    view_set_enter_callback(App->ViewEpc, uhf_reader_view_epc_enter_callback);
    view_set_exit_callback(App->ViewEpc, uhf_reader_view_epc_exit_callback);
    view_set_context(App->ViewEpc, App);

    view_allocate_model(App->ViewEpc, ViewModelTypeLockFree, sizeof(UHFRFIDTagModel));
    UHFRFIDTagModel* ModelEpc = view_get_model(App->ViewEpc);
    ModelEpc->Epc = furi_string_alloc_set("No Tags read");
    ModelEpc->Tid = furi_string_alloc_set("---");
    ModelEpc->User = furi_string_alloc_set("---");
    ModelEpc->Reserved = furi_string_alloc_set("---");
    ModelEpc->Crc = furi_string_alloc_set("----");
    ModelEpc->Pc = furi_string_alloc_set("----");
    ModelEpc->IsDeepReading = false;
    ModelEpc->DeepReadDone = false;
    ModelEpc->Rssi = 0;

    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewEpcDump, App->ViewEpc);
}

void view_epc_free(UHFReaderApp* App) {
    if(!App->ViewEpc) return;

    UHFRFIDTagModel* model = view_get_model(App->ViewEpc);
    uhf_tag_model_strings_free(model);

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewEpcDump);
    view_free(App->ViewEpc);
    App->ViewEpc = NULL;
}
