#include "terminal_view.h"
#include <gui/canvas.h>
#include <gui/elements.h>

#define TAG "Terminal View"

struct TerminalView {
    View* view;
};

typedef struct {
    uint8_t buffer[4096];
    uint8_t* tail;
    size_t size;
    size_t scroll_offset;
    FuriString* tmp_str;
    TerminalDisplayMode display_mode;
} TerminalViewModel;

#define TERMINAL_VIEW_CONTEXT_TO_TERMINAL(context) \
    furi_check(context);                           \
    TerminalView* terminal = context;

#define TERMINAL_VIEW_CONTEXT_TO_TERMINAL_AND_VIEW(context) \
    TERMINAL_VIEW_CONTEXT_TO_TERMINAL(context);             \
    furi_check(terminal->view);                             \
    View* view = terminal->view;

#define TERMINAL_VIEW_CONTEXT_TO_MODEL(context) \
    furi_check(context);                        \
    TerminalViewModel* model = context;

typedef struct {
    size_t width;
    size_t height;
    size_t glyph_width;
    size_t glyph_height;
    size_t frame_width;
    size_t frame_height;
    size_t frame_padding;
    size_t frame_body_width;
    size_t frame_body_height;
    size_t rows;
    size_t columns;
} TerminalViewDrawInfo;

typedef struct {
    size_t position;
    size_t total;
} TerminalViewScrollInfo;

typedef void (*TerminalViewDrawTableAddByteToStrCallback)(uint8_t byte, FuriString* str);

typedef void (*TerminalViewDrawTableAddEndOfRowCallback)(
    TerminalViewModel* model,
    uint8_t* start,
    size_t row,
    size_t byte_per_row,
    size_t byte_in_row,
    FuriString* str);

static bool terminal_view_byte_is_printable(uint8_t b) {
    return (b >= '!' && b <= '~');
}

static inline uint8_t* terminal_view_wrap_buffer_pointer(TerminalViewModel* model, uint8_t* ptr) {
    int pos = ptr - model->buffer;

    int offset_from_start_of_buffer = pos % sizeof(model->buffer);

    return model->buffer + offset_from_start_of_buffer;
}

static inline uint8_t* terminal_view_buffer_get_byte_form_start(
    TerminalViewModel* model,
    uint8_t* start,
    size_t offset) {
    return terminal_view_wrap_buffer_pointer(model, start + offset);
}

static inline uint8_t terminal_view_get_byte_value_from_start(
    TerminalViewModel* model,
    uint8_t* start,
    size_t offset) {
    uint8_t* ptr = terminal_view_buffer_get_byte_form_start(model, start, offset);
    return *ptr;
}

static uint8_t* terminal_view_get_start_of_data(TerminalViewModel* model, size_t bytes_in_row) {
    uint8_t* start;
    if(model->size != sizeof(model->buffer)) {
        start = model->buffer;
    } else {
        start = model->tail;
    }

    start = start + (model->scroll_offset * bytes_in_row);

    return terminal_view_wrap_buffer_pointer(model, start);
}

static inline void terminal_view_draw_table_row(
    Canvas* canvas,
    TerminalViewModel* model,
    const TerminalViewDrawInfo* info,
    size_t row,
    uint8_t* start,
    size_t byte_per_row,
    size_t byte_in_row,
    size_t chars_per_byte,
    FuriString* str,
    TerminalViewDrawTableAddEndOfRowCallback add_end_of_row_cb) {
    const size_t x = info->frame_padding;

    const size_t y = info->frame_padding + // padding from top
                     info->glyph_height + // strings start to drawing from the bottom
                     (info->glyph_height * row); // offset for row

    for(size_t i = byte_in_row; i < byte_per_row; i++) {
        for(size_t j = 0; j < chars_per_byte; j++) {
            furi_string_push_back(str, ' ');
        }
        furi_string_push_back(str, ' ');
    }

    if(add_end_of_row_cb) {
        add_end_of_row_cb(model, start, row, byte_per_row, byte_in_row, str);
    }

    canvas_draw_str(canvas, x, y, furi_string_get_cstr(str));
}

static inline size_t
    terminal_view_draw_table_calculate_total_numer_of_rows(size_t size, size_t per_row) {
    size_t full_rows = size / per_row;
    if(size % per_row == 0) { //All rows are full
        return full_rows;
    } else {
        return full_rows + 1;
    }
}

static TerminalViewScrollInfo terminal_view_draw_table(
    Canvas* canvas,
    TerminalViewModel* model,
    const TerminalViewDrawInfo* info,
    size_t bytes_per_row,
    size_t chars_per_byte,
    const char* separator,
    TerminalViewDrawTableAddByteToStrCallback add_byte_to_str_cb,
    TerminalViewDrawTableAddEndOfRowCallback add_end_of_row_cb) {
    const size_t bytes_on_screen = bytes_per_row * info->rows; // max number of bytes on screen
    const size_t total_numer_of_rows =
        terminal_view_draw_table_calculate_total_numer_of_rows(model->size, bytes_per_row);
    if(model->scroll_offset + info->rows > total_numer_of_rows) {
        if(total_numer_of_rows < info->rows) {
            model->scroll_offset = 0;
        } else {
            model->scroll_offset = total_numer_of_rows - info->rows;
        }
    }

    uint8_t* start = terminal_view_get_start_of_data(model, bytes_per_row);

    furi_string_reset(model->tmp_str);
    size_t to_print =
        MIN(model->size - (model->scroll_offset * bytes_per_row),
            bytes_on_screen); // how many chars need to be printed
    size_t current_row = 0; // offset of current row
    size_t in_row = 0; // printed number of bytes in current row
    size_t offset = 0; // offset of current byte
    while(to_print > 0) {
        uint8_t b = terminal_view_get_byte_value_from_start(model, start, offset);

        add_byte_to_str_cb(b, model->tmp_str);

        furi_string_cat_str(model->tmp_str, separator); // separator between bytes/chars

        offset++;
        to_print--;

        in_row++;
        if(in_row >= bytes_per_row) { // end of row reached
            terminal_view_draw_table_row(
                canvas,
                model,
                info,
                current_row,
                start,
                bytes_per_row,
                in_row,
                chars_per_byte,
                model->tmp_str,
                add_end_of_row_cb);
            furi_string_reset(model->tmp_str);

            in_row = 0;
            current_row++;
        }
    }

    if(in_row > 0) {
        terminal_view_draw_table_row(
            canvas,
            model,
            info,
            current_row,
            start,
            bytes_per_row,
            in_row,
            chars_per_byte,
            model->tmp_str,
            add_end_of_row_cb);
    }

    TerminalViewScrollInfo ret = {
        .position = model->scroll_offset,
        .total = total_numer_of_rows - info->rows + 1,
    };
    return ret;
}

static void terminal_view_draw_auto_add_byte_to_str(uint8_t byte, FuriString* str) {
    if(terminal_view_byte_is_printable(byte) && byte != '"' && byte != '\\' && byte != ' ' &&
       byte != '\'') {
        furi_string_push_back(str, byte);
        furi_string_push_back(str, ' ');
    } else {
        if(byte == '\a') {
            furi_string_cat_str(str, "\\a");
        } else if(byte == '\b') {
            furi_string_cat_str(str, "\\b");
        } else if(byte == '\e') {
            furi_string_cat_str(str, "\\e");
        } else if(byte == '\f') {
            furi_string_cat_str(str, "\\f");
        } else if(byte == '\n') {
            furi_string_cat_str(str, "\\n");
        } else if(byte == '\r') {
            furi_string_cat_str(str, "\\r");
        } else if(byte == '\t') {
            furi_string_cat_str(str, "\\t");
        } else if(byte == '\v') {
            furi_string_cat_str(str, "\\v");
        } else if(byte == '\\') {
            furi_string_cat_str(str, "\\\\");
        } else if(byte == '\'') {
            furi_string_cat_str(str, "\\'");
        } else if(byte == '\"') {
            furi_string_cat_str(str, "\\\"");
        } else if(byte == ' ') {
            furi_string_cat_str(str, "\\ ");
        } else {
            furi_string_cat_printf(str, "%02X", byte);
        }
    }
}

static inline TerminalViewScrollInfo terminal_view_draw_auto(
    Canvas* canvas,
    TerminalViewModel* model,
    const TerminalViewDrawInfo* info) {
    return terminal_view_draw_table(
        canvas,
        model,
        info,
        info->columns / 2,
        2,
        "",
        terminal_view_draw_auto_add_byte_to_str,
        NULL);
}

static void terminal_view_draw_text_add_byte_to_str(uint8_t byte, FuriString* str) {
    if(terminal_view_byte_is_printable(byte)) {
        furi_string_push_back(str, byte);
    } else {
        furi_string_push_back(str, ' ');
    }
}

static inline TerminalViewScrollInfo terminal_view_draw_text(
    Canvas* canvas,
    TerminalViewModel* model,
    const TerminalViewDrawInfo* info) {
    return terminal_view_draw_table(
        canvas, model, info, info->columns, 1, "", terminal_view_draw_text_add_byte_to_str, NULL);
}

static void terminal_view_draw_hex_add_byte_to_string(uint8_t byte, FuriString* str) {
    furi_string_cat_printf(str, "%02X", byte);
}

static void terminal_view_draw_hex_append_data_as_string(
    TerminalViewModel* model,
    uint8_t* start,
    size_t row,
    size_t byte_per_row,
    size_t byte_in_row,
    FuriString* str) {
    furi_string_push_back(str, '|');
    furi_string_push_back(str, ' ');

    for(size_t i = 0; i < byte_in_row; i++) {
        uint8_t b =
            terminal_view_get_byte_value_from_start(model, start, (row * byte_per_row) + i);

        if(terminal_view_byte_is_printable(b)) {
            furi_string_push_back(str, b);
        } else {
            furi_string_push_back(str, ' ');
        }
    }
}

static inline TerminalViewScrollInfo terminal_view_draw_hex(
    Canvas* canvas,
    TerminalViewModel* model,
    const TerminalViewDrawInfo* info) {
    return terminal_view_draw_table(
        canvas,
        model,
        info,
        4,
        2,
        " ",
        terminal_view_draw_hex_add_byte_to_string,
        terminal_view_draw_hex_append_data_as_string);
}

static void terminal_view_draw_binary_add_byte_to_string(uint8_t byte, FuriString* str) {
    for(int i = 7; i >= 0; i--) {
        if(byte & (1 << i)) {
            furi_string_push_back(str, '1');
        } else {
            furi_string_push_back(str, '0');
        }
    }
}

static inline TerminalViewScrollInfo terminal_view_draw_binary(
    Canvas* canvas,
    TerminalViewModel* model,
    const TerminalViewDrawInfo* info) {
    return terminal_view_draw_table(
        canvas,
        model,
        info,
        info->columns / (8 + 1), // 8 => Bit per byte; 1 => Separator
        8,
        " ",
        terminal_view_draw_binary_add_byte_to_string,
        NULL);
}

static TerminalViewScrollInfo terminal_view_call_draw(
    Canvas* canvas,
    TerminalViewModel* model,
    const TerminalViewDrawInfo* info) {
    switch(model->display_mode) {
    case TerminalDisplayModeAuto:
        return terminal_view_draw_auto(canvas, model, info);

    case TerminalDisplayModeText:
        return terminal_view_draw_text(canvas, model, info);

    case TerminalDisplayModeHex:
        return terminal_view_draw_hex(canvas, model, info);

    case TerminalDisplayModeBinary:
        return terminal_view_draw_binary(canvas, model, info);

    default:
        furi_crash("Bad display mode!");
        break; //if you get here, I'll by you a cookie
    }
}

static void terminal_view_draw_callback(Canvas* canvas, void* context) {
    furi_check(canvas);
    TERMINAL_VIEW_CONTEXT_TO_MODEL(context);

    canvas_set_font(canvas, FontKeyboard);

    TerminalViewDrawInfo info = {0};
    info.width = canvas_width(canvas);
    info.height = canvas_height(canvas);
    info.glyph_height = canvas_current_font_height(canvas);
    info.glyph_width =
        6; // For some reason not supported with uFTB canvas_current_font_width(canvas);
    info.frame_width = info.width - 4; // 4 => scrollbar has a width of 3 + 1 space
    info.frame_height = info.height; // mainly a alias
    info.frame_padding = 2 + 1; // +1 => line with of frame
    info.frame_body_width = info.frame_width - (info.frame_padding * 2);
    info.frame_body_height = info.frame_height - (info.frame_padding * 2);
    info.rows = info.frame_body_height / info.glyph_height;
    info.columns = info.frame_body_width / info.glyph_width;

    elements_slightly_rounded_frame(canvas, 0, 0, info.frame_width, info.frame_height);

    TerminalViewScrollInfo scroll_bar_draw_info = terminal_view_call_draw(canvas, model, &info);
    elements_scrollbar(canvas, scroll_bar_draw_info.position, scroll_bar_draw_info.total);

    FURI_LOG_T(
        TAG,
        "Scrolling:\n"
        "\tTotal: %zu\n"
        "\tPosition: %zu",
        scroll_bar_draw_info.total,
        scroll_bar_draw_info.position);
}

static bool terminal_view_input_callback(InputEvent* event, void* context) {
    TERMINAL_VIEW_CONTEXT_TO_TERMINAL_AND_VIEW(context);

    if((event->key == InputKeyUp || event->key == InputKeyDown) &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        bool handled = false;
        with_view_model(
            view,
            TerminalViewModel * model,
            {
                size_t old_offset = model->scroll_offset;

                if(event->key == InputKeyUp) {
                    if(model->scroll_offset != 0) {
                        model->scroll_offset--;
                    }
                } else if(event->key == InputKeyDown) {
                    model->scroll_offset++;
                }

                handled = model->scroll_offset != old_offset;
            },
            handled);

        return handled;
    } else if(event->key == InputKeyBack && event->type == InputTypeLong) {
        terminal_view_reset(terminal);
        return true;
    }

    return false;
}

TerminalView* terminal_view_alloc() {
    TerminalView* terminal = malloc(sizeof(TerminalView));

    terminal->view = view_alloc();
    view_set_context(terminal->view, terminal);
    view_allocate_model(terminal->view, ViewModelTypeLockFree, sizeof(TerminalViewModel));
    view_set_draw_callback(terminal->view, terminal_view_draw_callback);
    view_set_input_callback(terminal->view, terminal_view_input_callback);

    with_view_model(
        terminal->view,
        TerminalViewModel * model,
        {
            model->display_mode = TerminalDisplayModeText;
            model->tail = model->buffer;
            model->size = 0;
            model->scroll_offset = 0;

            model->tmp_str = furi_string_alloc();
            furi_string_reserve(model->tmp_str, 64);
        },
        true);

    return terminal;
}

View* terminal_view_get_view(TerminalView* terminal) {
    furi_check(terminal);
    furi_check(terminal->view);
    return terminal->view;
}

void terminal_view_free(TerminalView* terminal) {
    furi_check(terminal);

    with_view_model(
        terminal->view, TerminalViewModel * model, { furi_string_free(model->tmp_str); }, true);

    furi_check(terminal->view);
    view_free(terminal->view);

    free(terminal);
}

void terminal_view_reset(TerminalView* terminal) {
    furi_check(terminal);

    with_view_model(
        terminal->view,
        TerminalViewModel * model,
        {
            model->tail = model->buffer;
            model->size = 0;
            model->scroll_offset = 0;
        },
        true);
}

void terminal_view_set_display_mode(TerminalView* terminal, TerminalDisplayMode mode) {
    furi_check(terminal);
    furi_check(mode < TerminalDisplayModeMax);

    with_view_model(
        terminal->view, TerminalViewModel * model, { model->display_mode = mode; }, true);
}

void terminal_view_debug_print_buffer(TerminalView* terminal) {
    with_view_model(
        terminal->view,
        TerminalViewModel * model,
        {
            for(size_t i = 0; i < sizeof(model->buffer); i++) {
                uint8_t c = model->buffer[i];
                printf("\n%3zu: %02X", i, c);

                if((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    printf(" (%c)", c);
                }
            }
        },
        false);
}

static bool
    terminal_view_read_data_from_stream(TerminalViewModel* model, FuriStreamBuffer* stream) {
    const uint8_t* end_of_buffer = model->buffer + sizeof(model->buffer);
    size_t free = end_of_buffer - model->tail;
    if(free == 0) {
        free = sizeof(model->buffer);
        model->tail = model->buffer;
    }

    size_t read = furi_stream_buffer_receive(stream, model->tail, free, 0);

    if(read == 0) {
        return false;
    }

    model->size = model->size + read;
    if(model->size > sizeof(model->buffer)) {
        model->size = sizeof(model->buffer);
    }

    model->tail = terminal_view_wrap_buffer_pointer(model, model->tail + read);

    return true;
}

void terminal_view_append_data_from_stream(TerminalView* terminal, FuriStreamBuffer* stream) {
    furi_check(terminal);
    furi_check(stream);

    bool update;
    with_view_model(
        terminal->view,
        TerminalViewModel * model,
        { update = terminal_view_read_data_from_stream(model, stream); },
        update)
}
