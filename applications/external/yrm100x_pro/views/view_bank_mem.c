#include "view_bank_mem.h"

// Layout constants for the wrapped hex display.
#define BANK_CHARS_PER_LINE 20
#define BANK_VISIBLE_LINES  4
#define BANK_CONTENT_TOP_Y  22
#define BANK_LINE_STEP_Y    10

// ─── Bank helpers ─────────────────────────────────────────────────────────────
static const char* bank_mem_name(uint32_t bank) {
    switch(bank) {
    case 0:
        return "TID";
    case 1:
        return "Reserved";
    default:
        return "User Mem";
    }
}

// Short bank labels for the browse buttons (must fit the narrow button width).
static const char* bank_mem_short_name(uint32_t bank) {
    switch(bank) {
    case 0:
        return "TID";
    case 1:
        return "Res";
    default:
        return "Usr";
    }
}

static BankType bank_mem_type(uint32_t bank) {
    switch(bank) {
    case 0:
        return TIDBank;
    case 1:
        return ReservedBank;
    default:
        return UserBank;
    }
}

static FuriString* bank_mem_string(UHFRFIDTagModel* model) {
    switch(model->CurrentBank) {
    case 0:
        return model->Tid;
    case 1:
        return model->Reserved;
    default:
        return model->User;
    }
}

// ─── 5-second single-bank read timeout ───────────────────────────────────────
static void uhf_bank_mem_timeout_callback(void* ctx) {
    UHFReaderApp* App = (UHFReaderApp*)ctx;
    App->DeepReadTimerExpired = true;
    uhf_worker_change_state(App->YRM100XWorker, UHFWorkerStateStop);
}

// ─── Draw callback ────────────────────────────────────────────────────────────
void uhf_reader_view_bank_mem_draw_callback(Canvas* canvas, void* model) {
    UHFRFIDTagModel* MyModel = (UHFRFIDTagModel*)model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header: bank name + decoded byte count.
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 11, bank_mem_name(MyModel->CurrentBank));
    canvas_set_font(canvas, FontSecondary);

    const char* HeaderContent = furi_string_get_cstr(bank_mem_string(MyModel));
    size_t HeaderHexLen = strlen(HeaderContent);
    size_t HeaderBytes = HeaderHexLen / 2U;
    char SizeLabel[18];
    snprintf(SizeLabel, sizeof(SizeLabel), "%uB", (unsigned int)HeaderBytes);
    canvas_draw_str_aligned(canvas, 128, 11, AlignRight, AlignBottom, SizeLabel);

    // Busy state: single-bank read in progress (no browsing while reading).
    if(MyModel->IsDeepReading) {
        canvas_draw_str(canvas, 34, 40, "Reading...");
        return;
    }

    // Bank browse ring buttons: Left = previous node, Right = next node.
    // TID's previous and User's next are both the EPC dump screen.
    elements_button_left(
        canvas, MyModel->CurrentBank == 0 ? "EPC" : bank_mem_short_name(MyModel->CurrentBank - 1));
    elements_button_right(
        canvas, MyModel->CurrentBank == 2 ? "EPC" : bank_mem_short_name(MyModel->CurrentBank + 1));

    const char* Content = furi_string_get_cstr(bank_mem_string(MyModel));
    size_t Len = strlen(Content);

    if(Len == 0) {
        canvas_draw_str(canvas, 14, 40, "Press OK to read");
        elements_button_center(canvas, "Read");
        return;
    }

    // Full content, wrapped, no horizontal scroll; vertical paging via VScrollLine.
    size_t TotalLines = (Len + BANK_CHARS_PER_LINE - 1) / BANK_CHARS_PER_LINE;
    // Reserved bank has 2 extra virtual lines below the hex: Kill and Access passwords.
    bool IsReserved = (MyModel->CurrentBank == 1);
    size_t ExtendedTotal = IsReserved ? TotalLines + 2 : TotalLines;
    uint32_t Start = MyModel->VScrollLine;

    for(uint32_t Row = 0; Row < BANK_VISIBLE_LINES; Row++) {
        size_t LineIdx = Start + Row;
        if(LineIdx >= ExtendedTotal) break;
        char LineBuf[BANK_CHARS_PER_LINE + 1];
        if(IsReserved && LineIdx >= TotalLines) {
            // Extra lines: decoded Kill Password (bytes 0-3) and Access Password (bytes 4-7).
            size_t ExtraIdx = LineIdx - TotalLines;
            if(ExtraIdx == 0) {
                snprintf(
                    LineBuf, sizeof(LineBuf), "KP: %.*s", (int)(Len >= 8 ? 8 : (int)Len), Content);
            } else {
                if(Len >= 8) {
                    snprintf(
                        LineBuf,
                        sizeof(LineBuf),
                        "AP: %.*s",
                        (int)(Len >= 16 ? 8 : (int)(Len - 8)),
                        Content + 8);
                } else {
                    snprintf(LineBuf, sizeof(LineBuf), "AP:  (n/a)");
                }
            }
        } else {
            size_t Offset = LineIdx * BANK_CHARS_PER_LINE;
            size_t N = (Len - Offset) < BANK_CHARS_PER_LINE ? (Len - Offset) : BANK_CHARS_PER_LINE;
            memcpy(LineBuf, Content + Offset, N);
            LineBuf[N] = '\0';
        }
        canvas_draw_str(canvas, 0, BANK_CONTENT_TOP_Y + Row * BANK_LINE_STEP_Y, LineBuf);
    }

    // Vertical scroll indicators (far right so they clear the 20-char content).
    if(Start > 0) {
        canvas_draw_str(canvas, 122, 30, "\x18");
    }
    if(Start + BANK_VISIBLE_LINES < ExtendedTotal) {
        canvas_draw_str(canvas, 122, 52, "\x19");
    }

    elements_button_center(canvas, "Read");
}

// ─── Navigation ───────────────────────────────────────────────────────────────
uint32_t uhf_reader_navigation_bank_mem_to_epc_dump_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewEpcDump;
}

// ─── Worker callback (single-bank read) ──────────────────────────────────────
void uhf_single_bank_read_worker_callback(UHFWorkerEvent event, void* ctx) {
    UHFReaderApp* App = (UHFReaderApp*)ctx;
    // Success, or the timer expired (show whatever was read), both count as done.
    if(event == UHFWorkerEventSuccess ||
       (event == UHFWorkerEventAborted && App->DeepReadTimerExpired)) {
        if(event == UHFWorkerEventSuccess) {
            uhf_notify_success(App);
        }
        App->DeepReadTimerExpired = false;
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventSingleReadDone);
    } else {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventSingleReadAborted);
    }
}

// ─── Input handler ────────────────────────────────────────────────────────────
bool uhf_reader_view_bank_mem_input_callback(InputEvent* event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    if(event->type != InputTypeShort) return false;

    // Back during a read: cancel the worker (stay on screen).
    if(event->key == InputKeyBack && App->DeepReading) {
        uhf_worker_change_state(App->YRM100XWorker, UHFWorkerStateStop);
        return true;
    }

    // OK: read (or re-read) the currently shown bank.
    if(event->key == InputKeyOk && !App->DeepReading && App->NumberOfEpcsToRead > 0) {
        uint32_t Cur = 0;
        with_view_model(App->ViewBankMem, UHFRFIDTagModel * m, { Cur = m->CurrentBank; }, false);

        App->DeepReading = true;
        App->DeepReadTimerExpired = false;
        App->YRM100XWorker->TargetBank = bank_mem_type(Cur);

        with_view_model(App->ViewBankMem, UHFRFIDTagModel * m, { m->IsDeepReading = true; }, true);

        notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);

        uhf_worker_stop(App->YRM100XWorker);
        uhf_worker_start(
            App->YRM100XWorker,
            UHFWorkerStateReadSingleBank,
            uhf_single_bank_read_worker_callback,
            App);

        if(App->Timer) {
            furi_timer_stop(App->Timer);
            furi_timer_free(App->Timer);
            App->Timer = NULL;
        }
        App->Timer = furi_timer_alloc(uhf_bank_mem_timeout_callback, FuriTimerTypeOnce, App);
        furi_timer_start(App->Timer, furi_ms_to_ticks(5000));
        return true;
    }

    // Right: browse forward to the next bank; wrap User → EPC dump.
    if(event->key == InputKeyRight && !App->DeepReading) {
        uint32_t Cur = 0;
        with_view_model(App->ViewBankMem, UHFRFIDTagModel * m, { Cur = m->CurrentBank; }, false);
        if(Cur < 2) {
            with_view_model(
                App->ViewBankMem,
                UHFRFIDTagModel * m,
                {
                    m->CurrentBank = Cur + 1;
                    m->VScrollLine = 0;
                },
                true);
        } else {
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewEpcDump);
        }
        return true;
    }

    // Left: browse backward to the previous bank; wrap TID → EPC dump.
    if(event->key == InputKeyLeft && !App->DeepReading) {
        uint32_t Cur = 0;
        with_view_model(App->ViewBankMem, UHFRFIDTagModel * m, { Cur = m->CurrentBank; }, false);
        if(Cur > 0) {
            with_view_model(
                App->ViewBankMem,
                UHFRFIDTagModel * m,
                {
                    m->CurrentBank = Cur - 1;
                    m->VScrollLine = 0;
                },
                true);
        } else {
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewEpcDump);
        }
        return true;
    }

    // Down: scroll wrapped content down (bounded).
    if(event->key == InputKeyDown && !App->DeepReading) {
        with_view_model(
            App->ViewBankMem,
            UHFRFIDTagModel * m,
            {
                size_t Len = strlen(furi_string_get_cstr(bank_mem_string(m)));
                size_t Total = (Len + BANK_CHARS_PER_LINE - 1) / BANK_CHARS_PER_LINE;
                if(m->CurrentBank == 1) Total += 2; // Reserved: +Kill/Access lines
                size_t MaxStart = Total > BANK_VISIBLE_LINES ? Total - BANK_VISIBLE_LINES : 0;
                if(m->VScrollLine < MaxStart) m->VScrollLine++;
            },
            true);
        return true;
    }

    // Up: scroll wrapped content up.
    if(event->key == InputKeyUp && !App->DeepReading) {
        with_view_model(
            App->ViewBankMem,
            UHFRFIDTagModel * m,
            {
                if(m->VScrollLine > 0) m->VScrollLine--;
            },
            true);
        return true;
    }

    return false;
}

// ─── Custom event handler ─────────────────────────────────────────────────────
bool uhf_reader_view_bank_mem_custom_event_callback(uint32_t event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    switch(event) {
    case UHFReaderEventIdRedrawScreen: {
        with_view_model(App->ViewBankMem, UHFRFIDTagModel * _m, { UNUSED(_m); }, true);
        return true;
    }

    case UHFCustomEventSingleReadDone: {
        if(App->Timer) {
            furi_timer_stop(App->Timer);
            furi_timer_free(App->Timer);
            App->Timer = NULL;
        }
        App->DeepReading = false;

        uint32_t Cur = 0;
        with_view_model(App->ViewBankMem, UHFRFIDTagModel * m, { Cur = m->CurrentBank; }, false);

        UHFTag* tag = App->YRM100XWorker->SelectedTag;
        char* Hex = NULL;
        if(Cur == 0) {
            Hex = convertToHexString(tag->tid->data, tag->tid->size);
        } else if(Cur == 2) {
            Hex = convertToHexString(tag->user->data, tag->user->size);
        } else {
            Hex = convertToHexString(tag->reserved->data, tag->reserved->size);
        }
        const char* Disp = (Hex && Hex[0] != '\0') ? Hex : "(empty)";

        // Update the bank memory screen model.
        with_view_model(
            App->ViewBankMem,
            UHFRFIDTagModel * m,
            {
                m->IsDeepReading = false;
                m->VScrollLine = 0;
                if(Cur == 0) {
                    furi_string_set_str(m->Tid, Disp);
                } else if(Cur == 1) {
                    furi_string_set_str(m->Reserved, Disp);
                } else {
                    furi_string_set_str(m->User, Disp);
                }
            },
            true);

        // Mirror into the EPC dump model so an Up-key Save there includes this bank.
        with_view_model(
            App->ViewEpc,
            UHFRFIDTagModel * em,
            {
                if(Cur == 0) {
                    furi_string_set_str(em->Tid, Disp);
                    em->TidBankRead = true;
                } else if(Cur == 1) {
                    furi_string_set_str(em->Reserved, Disp);
                    em->ResBankRead = true;
                } else {
                    furi_string_set_str(em->User, Disp);
                    em->UserBankRead = true;
                }
            },
            false);

        if(Hex) free(Hex);
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        return true;
    }

    case UHFCustomEventSingleReadAborted: {
        if(App->Timer) {
            furi_timer_stop(App->Timer);
            furi_timer_free(App->Timer);
            App->Timer = NULL;
        }
        App->DeepReading = false;
        with_view_model(
            App->ViewBankMem, UHFRFIDTagModel * m, { m->IsDeepReading = false; }, true);
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        return true;
    }

    default:
        return false;
    }
}

// ─── Exit callback ────────────────────────────────────────────────────────────
void uhf_reader_view_bank_mem_exit_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    // Stop a pending single-bank read timer if the user navigates away mid-read.
    if(App->Timer) {
        furi_timer_stop(App->Timer);
        furi_timer_free(App->Timer);
        App->Timer = NULL;
    }
    if(App->DeepReading) {
        uhf_worker_stop(App->YRM100XWorker);
        App->DeepReading = false;
        with_view_model(
            App->ViewBankMem, UHFRFIDTagModel * m, { m->IsDeepReading = false; }, false);
    }
}

// ─── Enter callback ───────────────────────────────────────────────────────────
void uhf_reader_view_bank_mem_enter_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    UHFTagWrapper* Wrapper = App->YRM100XWorker->uhf_tag_wrapper;
    uint32_t Sel = App->CurEpcIndex;

    // Seed SelectedTag with the currently displayed EPC so single-bank reads can
    // Select the right tag. Only clear cached bank strings when the tag changed.
    if(Sel >= 1 && Sel <= Wrapper->tag_count) {
        UHFTag* Selected = Wrapper->tags[Sel - 1];
        UHFTag* Target = App->YRM100XWorker->SelectedTag;
        uhf_tag_reset(Target);
        uhf_tag_set_epc(Target, Selected->epc->data, Selected->epc->size);
        uhf_tag_set_epc_pc(Target, Selected->epc->pc);
        uhf_tag_set_epc_crc(Target, Selected->epc->crc);
    }

    bool NewTag = false;
    with_view_model(
        App->ViewBankMem, UHFRFIDTagModel * m, { NewTag = (m->CurEpcIndex != Sel); }, false);

    with_view_model(
        App->ViewBankMem,
        UHFRFIDTagModel * m,
        {
            m->IsDeepReading = false;
            m->VScrollLine = 0;
            if(NewTag) {
                m->CurEpcIndex = Sel;
                furi_string_set_str(m->Tid, "");
                furi_string_set_str(m->Reserved, "");
                furi_string_set_str(m->User, "");
            }
        },
        true);
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

// ─── Alloc / Free ─────────────────────────────────────────────────────────────
void view_bank_mem_alloc(UHFReaderApp* App) {
    App->ViewBankMem = view_alloc();
    view_set_draw_callback(App->ViewBankMem, uhf_reader_view_bank_mem_draw_callback);
    view_set_input_callback(App->ViewBankMem, uhf_reader_view_bank_mem_input_callback);
    view_set_previous_callback(
        App->ViewBankMem, uhf_reader_navigation_bank_mem_to_epc_dump_callback);
    view_set_enter_callback(App->ViewBankMem, uhf_reader_view_bank_mem_enter_callback);
    view_set_exit_callback(App->ViewBankMem, uhf_reader_view_bank_mem_exit_callback);
    view_set_custom_callback(App->ViewBankMem, uhf_reader_view_bank_mem_custom_event_callback);
    view_set_context(App->ViewBankMem, App);

    view_allocate_model(App->ViewBankMem, ViewModelTypeLockFree, sizeof(UHFRFIDTagModel));
    UHFRFIDTagModel* Model = view_get_model(App->ViewBankMem);
    Model->Epc = furi_string_alloc();
    Model->Tid = furi_string_alloc();
    Model->Reserved = furi_string_alloc();
    Model->User = furi_string_alloc();
    Model->Crc = furi_string_alloc();
    Model->Pc = furi_string_alloc();
    Model->CurrentBank = 0;
    Model->VScrollLine = 0;
    Model->CurEpcIndex = 0;
    Model->IsDeepReading = false;
    Model->DeepReadDone = false;
    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewBankMem, App->ViewBankMem);
}

void view_bank_mem_free(UHFReaderApp* App) {
    if(!App->ViewBankMem) return;

    UHFRFIDTagModel* model = view_get_model(App->ViewBankMem);
    uhf_tag_model_strings_free(model);

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewBankMem);
    view_free(App->ViewBankMem);
    App->ViewBankMem = NULL;
}
