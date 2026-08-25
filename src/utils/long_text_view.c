#include "long_text_view.h"

#include <gui/elements.h>
#include <furi.h>

#define LONG_TEXT_WIDTH    120
#define LONG_TEXT_TOP_Y    11
#define LONG_TEXT_BOTTOM_Y 64
#define LONG_TEXT_LEFT_X   3
struct LongTextView {
    View* view;
};
typedef struct {
    FuriString* text;
    FuriString* line_scratch;
    uint16_t* line_starts; // Offset of the first char of each line
    size_t line_count;
    int32_t scroll_line;
    uint8_t lines_per_screen;
    bool dirty;
} LongTextViewModel;

/** Count the display lines: split on '\n' and wrap at LONG_TEXT_WIDTH. */
static size_t long_text_view_count_lines(Canvas* canvas, const char* text) {
    size_t lines = 1;
    size_t width = 0;
    for(const char* p = text; *p != '\0'; p++) {
        if(*p == '\n') {
            lines++;
            width = 0;
        } else {
            width += canvas_glyph_width(canvas, *p);
            if(width > LONG_TEXT_WIDTH) {
                lines++;
                width = canvas_glyph_width(canvas, *p);
            }
        }
    }
    return lines;
}

/** Fill line_starts with the offset of every line (entry line_count = size). */
static void
    long_text_view_build_lines(Canvas* canvas, LongTextViewModel* model, size_t text_size) {
    const char* text = furi_string_get_cstr(model->text);

    free(model->line_starts);
    model->line_starts = malloc((model->line_count + 1) * sizeof(uint16_t));

    size_t width = 0;
    size_t line = 0;
    model->line_starts[line++] = 0;
    for(size_t i = 0; i < text_size; i++) {
        char c = text[i];
        if(c == '\n') {
            model->line_starts[line++] = (uint16_t)(i + 1);
            width = 0;
        } else {
            width += canvas_glyph_width(canvas, c);
            if(width > LONG_TEXT_WIDTH) {
                model->line_starts[line++] = (uint16_t)i;
                width = canvas_glyph_width(canvas, c);
            }
        }
    }
    model->line_starts[line] = (uint16_t)text_size;
}

static void long_text_view_draw(Canvas* canvas, void* context) {
    furi_assert(context);
    LongTextViewModel* model = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    uint8_t font_height = canvas_current_font_height(canvas);
    model->lines_per_screen = (LONG_TEXT_BOTTOM_Y - LONG_TEXT_TOP_Y) / font_height;

    if(model->dirty) {
        model->line_count = long_text_view_count_lines(canvas, furi_string_get_cstr(model->text));
        long_text_view_build_lines(canvas, model, furi_string_size(model->text));
        model->dirty = false;
    }

    // Clamp the scroll position
    int32_t max_scroll = (int32_t)model->line_count - model->lines_per_screen;
    if(model->scroll_line > max_scroll) {
        model->scroll_line = max_scroll;
    }
    if(model->scroll_line < 0) {
        model->scroll_line = 0;
    }

    const char* text = furi_string_get_cstr(model->text);
    uint8_t y = LONG_TEXT_TOP_Y;
    for(uint8_t i = 0; i < model->lines_per_screen; i++) {
        int32_t line = model->scroll_line + i;
        if(line >= (int32_t)model->line_count) {
            break;
        }
        size_t start = model->line_starts[line];
        size_t end = model->line_starts[line + 1];
        furi_string_set_strn(model->line_scratch, text + start, end - start);
        canvas_draw_str(canvas, LONG_TEXT_LEFT_X, y, furi_string_get_cstr(model->line_scratch));
        y += font_height;
    }

    if(model->line_count > model->lines_per_screen) {
        elements_scrollbar(
            canvas, (size_t)model->scroll_line, model->line_count - model->lines_per_screen + 1);
    }
}

static bool long_text_view_input(InputEvent* event, void* context) {
    furi_assert(context);
    LongTextView* long_text_view = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        with_view_model(
            long_text_view->view,
            LongTextViewModel * model,
            {
                int32_t max_scroll = (int32_t)model->line_count - model->lines_per_screen;
                if(event->key == InputKeyUp) {
                    if(model->scroll_line > 0) {
                        model->scroll_line--;
                    }
                    consumed = true;
                } else if(event->key == InputKeyDown) {
                    if(model->scroll_line < max_scroll) {
                        model->scroll_line++;
                    }
                    consumed = true;
                } else if(event->key == InputKeyOk) {
                    // Page down
                    if(model->scroll_line < max_scroll) {
                        model->scroll_line += model->lines_per_screen;
                        if(model->scroll_line > max_scroll) {
                            model->scroll_line = max_scroll;
                        }
                    }
                    consumed = true;
                }
            },
            true);
    }

    return consumed;
}

LongTextView* long_text_view_alloc(void) {
    LongTextView* long_text_view = malloc(sizeof(LongTextView));

    long_text_view->view = view_alloc();
    view_set_context(long_text_view->view, long_text_view);
    view_allocate_model(long_text_view->view, ViewModelTypeLocking, sizeof(LongTextViewModel));
    view_set_draw_callback(long_text_view->view, long_text_view_draw);
    view_set_input_callback(long_text_view->view, long_text_view_input);

    with_view_model(
        long_text_view->view,
        LongTextViewModel * model,
        {
            model->text = furi_string_alloc();
            model->line_scratch = furi_string_alloc();
            model->line_starts = NULL;
            model->line_count = 0;
            model->scroll_line = 0;
            model->lines_per_screen = 0;
            model->dirty = true;
        },
        true);

    return long_text_view;
}

void long_text_view_free(LongTextView* long_text_view) {
    furi_check(long_text_view);

    with_view_model(
        long_text_view->view,
        LongTextViewModel * model,
        {
            furi_string_free(model->text);
            furi_string_free(model->line_scratch);
            free(model->line_starts);
        },
        false);

    view_free(long_text_view->view);
    free(long_text_view);
}

View* long_text_view_get_view(LongTextView* long_text_view) {
    furi_check(long_text_view);
    return long_text_view->view;
}

void long_text_view_set_text(LongTextView* long_text_view, const char* text) {
    furi_check(long_text_view);
    furi_check(text);

    with_view_model(
        long_text_view->view,
        LongTextViewModel * model,
        {
            furi_string_set_str(model->text, text);
            model->dirty = true;
            model->scroll_line = 0;
        },
        true);
}

void long_text_view_reset(LongTextView* long_text_view) {
    furi_check(long_text_view);

    with_view_model(
        long_text_view->view,
        LongTextViewModel * model,
        {
            furi_string_reset(model->text);
            model->dirty = true;
            model->scroll_line = 0;
        },
        true);
}
