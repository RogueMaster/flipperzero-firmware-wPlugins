#include "view_epc_info.h"

// ─── Page indices ─────────────────────────────────────────────────────────────
#define PAGE_EPC   0
#define PAGE_TID   1
#define PAGE_USR   2
#define PAGE_RES   3
#define PAGE_COUNT 4

static const char* const PAGE_TITLES[PAGE_COUNT] = {"EPC", "TID", "USR", "RES"};

// [page][0=left label, 1=right label]
static const char* const PAGE_NAV[PAGE_COUNT][2] = {
    {"RES", "TID"},
    {"EPC", "USR"},
    {"TID", "RES"},
    {"USR", "EPC"},
};

#define EPC_INFO_CHARS_PER_LINE 20
#define EPC_INFO_VISIBLE_LINES  3
#define EPC_INFO_CONTENT_TOP_Y  20
#define EPC_INFO_LINE_STEP_Y    10

// ─── Helper: draw wrapped, scrollable value ─────────────────────────────────
static void draw_value_scrollable(Canvas* canvas, const char* value, uint32_t vscroll) {
    const size_t CPL = EPC_INFO_CHARS_PER_LINE;
    size_t len = strlen(value);
    if(len == 0) return;

    size_t totalLines = (len + CPL - 1) / CPL;
    if(totalLines <= EPC_INFO_VISIBLE_LINES) {
        // Fit all lines — no scroll needed
        for(size_t i = 0; i < totalLines; i++) {
            size_t off = i * CPL;
            size_t n = (len - off) < CPL ? (len - off) : CPL;
            char buf[CPL + 1];
            memcpy(buf, value + off, n);
            buf[n] = '\0';
            canvas_draw_str(
                canvas, 0, EPC_INFO_CONTENT_TOP_Y + (int)i * EPC_INFO_LINE_STEP_Y, buf);
        }
        return;
    }

    // Show visible window
    size_t start = vscroll;
    for(uint32_t row = 0; row < EPC_INFO_VISIBLE_LINES; row++) {
        size_t lineIdx = start + row;
        if(lineIdx >= totalLines) break;
        size_t off = lineIdx * CPL;
        size_t n = (len - off) < CPL ? (len - off) : CPL;
        char buf[CPL + 1];
        memcpy(buf, value + off, n);
        buf[n] = '\0';
        canvas_draw_str(canvas, 0, EPC_INFO_CONTENT_TOP_Y + row * EPC_INFO_LINE_STEP_Y, buf);
    }

    // Scroll indicators
    if(start > 0) {
        canvas_draw_str(canvas, 122, EPC_INFO_CONTENT_TOP_Y + 12, "\x18");
    }
    if(start + EPC_INFO_VISIBLE_LINES < totalLines) {
        canvas_draw_str(canvas, 122, EPC_INFO_CONTENT_TOP_Y + 34, "\x19");
    }
}

// ─── Draw callback ────────────────────────────────────────────────────────────
void uhf_reader_view_epc_info_draw_callback(Canvas* canvas, void* model) {
    UHFRFIDTagModel* m = (UHFRFIDTagModel*)model;
    uint32_t page = m->CurrentBank % PAGE_COUNT;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Bold bank name top-left
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 11, PAGE_TITLES[page]);
    canvas_set_font(canvas, FontSecondary);

    // Bank value — scrollable wrapped content
    const char* value = "---";
    uint32_t vscroll = 0;
    switch(page) {
    case PAGE_EPC:
        value = furi_string_get_cstr(m->Epc);
        break;
    case PAGE_TID:
        value = furi_string_get_cstr(m->Tid);
        break;
    case PAGE_USR:
        value = furi_string_get_cstr(m->User);
        vscroll = m->VScrollLine;
        break;
    case PAGE_RES:
        value = furi_string_get_cstr(m->Reserved);
        vscroll = m->VScrollLine;
        break;
    default:
        break;
    }
    draw_value_scrollable(canvas, value, vscroll);

    // EPC page: CRC + PC below scrollable area
    if(page == PAGE_EPC) {
        canvas_draw_str(canvas, 4, 48, "CRC:");
        canvas_draw_str(canvas, 28, 48, furi_string_get_cstr(m->Crc));
        canvas_draw_str(canvas, 70, 48, "PC:");
        canvas_draw_str(canvas, 88, 48, furi_string_get_cstr(m->Pc));
    }

    // Left / right navigation hints
    elements_button_left(canvas, PAGE_NAV[page][0]);
    elements_button_right(canvas, PAGE_NAV[page][1]);
}

// ─── Input handler ────────────────────────────────────────────────────────────
bool uhf_reader_view_epc_info_input_callback(InputEvent* event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    if(event->type != InputTypeShort) return false;

    // Determine current page for key routing
    uint32_t page = 0;
    with_view_model(
        App->ViewEpcInfo, UHFRFIDTagModel * m, { page = m->CurrentBank % PAGE_COUNT; }, false);

    // Up: scroll content up (USR and RES pages)
    if(event->key == InputKeyUp && (page == PAGE_USR || page == PAGE_RES)) {
        with_view_model(
            App->ViewEpcInfo,
            UHFRFIDTagModel * m,
            {
                if(m->VScrollLine > 0) m->VScrollLine--;
            },
            true);
        return true;
    }

    // Down: scroll content down (USR and RES pages)
    if(event->key == InputKeyDown && (page == PAGE_USR || page == PAGE_RES)) {
        with_view_model(
            App->ViewEpcInfo,
            UHFRFIDTagModel * m,
            {
                size_t len =
                    strlen(furi_string_get_cstr(page == PAGE_USR ? m->User : m->Reserved));
                size_t total = (len + EPC_INFO_CHARS_PER_LINE - 1) / EPC_INFO_CHARS_PER_LINE;
                size_t maxStart = total > EPC_INFO_VISIBLE_LINES ? total - EPC_INFO_VISIBLE_LINES :
                                                                   0;
                if(m->VScrollLine < maxStart) m->VScrollLine++;
            },
            true);
        return true;
    }

    if(event->key == InputKeyLeft) {
        with_view_model(
            App->ViewEpcInfo,
            UHFRFIDTagModel * m,
            { m->CurrentBank = (m->CurrentBank + PAGE_COUNT - 1) % PAGE_COUNT; },
            true);
        return true;
    }

    if(event->key == InputKeyRight) {
        with_view_model(
            App->ViewEpcInfo,
            UHFRFIDTagModel * m,
            { m->CurrentBank = (m->CurrentBank + 1) % PAGE_COUNT; },
            true);
        return true;
    }

    return false;
}

// ─── Navigation callback ──────────────────────────────────────────────────────
uint32_t uhf_reader_navigation_exit_epc_info_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewTagAction;
}

// ─── Timer callback (kept as dead stub for link compatibility) ────────────────
void uhf_reader_view_epc_info_timer_callback(void* context) {
    UNUSED(context);
}

// ─── Enter callback ───────────────────────────────────────────────────────────
void uhf_reader_view_epc_info_enter_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    // Always start on the EPC page
    with_view_model(
        App->ViewEpcInfo,
        UHFRFIDTagModel * m,
        {
            m->CurrentBank = PAGE_EPC;
            m->VScrollLine = 0;
        },
        false);

    // Load saved tag data from file
    FuriString* TempStr = furi_string_alloc();
    FuriString* TempTag = furi_string_alloc();

    if(flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        furi_string_printf(TempStr, "Tag%ld", App->SelectedTagIndex);
        if(flipper_format_read_string(App->EpcFile, furi_string_get_cstr(TempStr), TempTag)) {
            const char* s = furi_string_get_cstr(TempTag);
            char* xepc = extract_epc(s);
            char* xtid = extract_tid(s);
            char* xres = extract_res(s);
            char* xmem = extract_mem(s);
            char* xcrc = extract_crc(s);
            char* xpc = extract_pc(s);
            with_view_model(
                App->ViewEpcInfo,
                UHFRFIDTagModel * m,
                {
                    if(xepc) furi_string_set_str(m->Epc, xepc);
                    if(xtid) furi_string_set_str(m->Tid, xtid);
                    if(xres) furi_string_set_str(m->Reserved, xres);
                    if(xmem) furi_string_set_str(m->User, xmem);
                    if(xcrc) furi_string_set_str(m->Crc, xcrc);
                    if(xpc) furi_string_set_str(m->Pc, xpc);
                },
                true);
            if(xepc) free(xepc);
            if(xtid) free(xtid);
            if(xres) free(xres);
            if(xmem) free(xmem);
            if(xcrc) free(xcrc);
            if(xpc) free(xpc);
        }
        flipper_format_file_close(App->EpcFile);
    }

    furi_string_free(TempStr);
    furi_string_free(TempTag);
}

// ─── Exit callback ────────────────────────────────────────────────────────────
void uhf_reader_view_epc_info_exit_callback(void* context) {
    UNUSED(context);
    // No timer to clean up.
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
void view_epc_info_alloc(UHFReaderApp* App) {
    App->ViewEpcInfo = view_alloc();
    view_set_draw_callback(App->ViewEpcInfo, uhf_reader_view_epc_info_draw_callback);
    view_set_input_callback(App->ViewEpcInfo, uhf_reader_view_epc_info_input_callback);
    view_set_previous_callback(App->ViewEpcInfo, uhf_reader_navigation_exit_epc_info_callback);
    view_set_enter_callback(App->ViewEpcInfo, uhf_reader_view_epc_info_enter_callback);
    view_set_exit_callback(App->ViewEpcInfo, uhf_reader_view_epc_info_exit_callback);
    view_set_context(App->ViewEpcInfo, App);

    view_allocate_model(App->ViewEpcInfo, ViewModelTypeLockFree, sizeof(UHFRFIDTagModel));
    UHFRFIDTagModel* m = view_get_model(App->ViewEpcInfo);
    m->Epc = furi_string_alloc_set("---");
    m->Tid = furi_string_alloc_set("---");
    m->User = furi_string_alloc_set("---");
    m->Reserved = furi_string_alloc_set("---");
    m->Crc = furi_string_alloc_set("----");
    m->Pc = furi_string_alloc_set("----");
    m->CurrentBank = PAGE_EPC;
    m->VScrollLine = 0;

    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewEpcInfo, App->ViewEpcInfo);
}

void view_epc_info_free(UHFReaderApp* App) {
    if(!App->ViewEpcInfo) return;

    UHFRFIDTagModel* model = view_get_model(App->ViewEpcInfo);
    uhf_tag_model_strings_free(model);

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewEpcInfo);
    view_free(App->ViewEpcInfo);
    App->ViewEpcInfo = NULL;
}
