#include "widget_element_i.h"
#include <m-array.h>

#define WIDGET_ELEMENT_TEXT_SCROLL_BAR_OFFSET (4)

typedef struct {
    Align horizontal;
    Unicode* text;
} TextScrollLineArray;

ARRAY_DEF(TextScrollLineArray, TextScrollLineArray, M_POD_OPLIST)

typedef struct {
    TextScrollLineArray_t line_array;
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    Unicode* text;
    uint8_t scroll_pos_total;
    uint8_t scroll_pos_current;
    bool text_formatted;
} WidgetElementTextScrollModel;

static bool
    widget_element_text_scroll_process_ctrl_symbols(TextScrollLineArray* line, Unicode* text) {
    bool processed = false;

    do {
        if(text[0] != '\e') break;
        char ctrl_symbol = text[1];
        if(ctrl_symbol == 'c') {
            line->horizontal = AlignCenter;
        } else if(ctrl_symbol == 'r') {
            line->horizontal = AlignRight;
        }
        uni_right(text, 2);
        processed = true;
    } while(false);

    return processed;
}

void widget_element_text_scroll_add_line(WidgetElement* element, TextScrollLineArray* line) {
    WidgetElementTextScrollModel* model = element->model;
    TextScrollLineArray new_line;
    new_line.horizontal = line->horizontal;
    size_t len = uni_strlen(line->text);
    // FURI_LOG_D("Test", "l=%d", len);
    new_line.text = uni_alloc(len);
    uni_strcpy(new_line.text, line->text);
    TextScrollLineArray_push_back(model->line_array, new_line);
    // FURI_LOG_D("Test", "add line");
}

static void widget_element_text_scroll_fill_lines(Canvas* canvas, WidgetElement* element) {
    UNUSED(canvas);
    WidgetElementTextScrollModel* model = element->model;
    TextScrollLineArray line_tmp;
    bool all_text_processed = false;
    line_tmp.text = uni_alloc(50);
    bool reached_new_line = true;
    uint16_t total_height = 0;

    while(!all_text_processed) {
        if(reached_new_line) {
            // Set default line properties
            line_tmp.horizontal = AlignLeft;
            memset(line_tmp.text, 0, 50);
            // Process control symbols
            while(widget_element_text_scroll_process_ctrl_symbols(&line_tmp, model->text))
                ;
        }
        // Set canvas font
        CFontInfo_t font_info = cfont_get_info(cfont->font_size);
        total_height += font_info.px;
        if(total_height > model->height) {
            model->scroll_pos_total++;
        }

        uint8_t line_width = 0;
        uint16_t char_i = 0;
        while(true) {
            Unicode next_char = model->text[char_i++];
            // FURI_LOG_D("Test", "idx=%d next_char=%04X", char_i, next_char);
            if(next_char == '\0') {
                // FURI_LOG_D("Test", "end");
                uni_push_back(line_tmp.text, '\0');
                widget_element_text_scroll_add_line(element, &line_tmp);
                total_height += font_info.px;
                all_text_processed = true;
                break;
            } else if(next_char == '\n') {
                // FURI_LOG_D("Test", "nl");
                uni_push_back(line_tmp.text, '\0');
                widget_element_text_scroll_add_line(element, &line_tmp);
                uni_right(model->text, char_i);
                total_height += font_info.px;
                reached_new_line = true;
                break;
            } else {
                line_width += cfont_get_utf16_glyph_width(next_char);
                if(line_width > model->width) {
                    // FURI_LOG_D("Test", "wrp");
                    uni_push_back(line_tmp.text, '\0');
                    widget_element_text_scroll_add_line(element, &line_tmp);
                    uni_right(model->text, char_i - 1);
                    memset(line_tmp.text, 0, 50);
                    total_height += font_info.px;
                    reached_new_line = false;
                    break;
                } else {
                    // FURI_LOG_D("Test", "apd");
                    uni_push_back(line_tmp.text, next_char);
                }
            }
        }
    }
    uni_free(line_tmp.text);
}

static void widget_element_text_scroll_draw(Canvas* canvas, WidgetElement* element) {
    furi_assert(canvas);
    furi_assert(element);

    furi_mutex_acquire(element->model_mutex, FuriWaitForever);

    WidgetElementTextScrollModel* model = element->model;
    if(!model->text_formatted) {
        widget_element_text_scroll_fill_lines(canvas, element);
        model->text_formatted = true;
    }
    // FURI_LOG_D("Test", "line ok");
    uint8_t y = model->y;
    uint8_t x = model->x;
    uint16_t curr_line = 0;
    CFontInfo_t font_info = cfont_get_info(cfont->font_size);

    if(TextScrollLineArray_size(model->line_array)) {
        TextScrollLineArray_it_t it;
        for(TextScrollLineArray_it(it, model->line_array); !TextScrollLineArray_end_p(it);
            TextScrollLineArray_next(it), curr_line++) {
            if(curr_line < model->scroll_pos_current) continue;
            TextScrollLineArray* line = TextScrollLineArray_ref(it);

            if(y + font_info.px > model->y + model->height) break;

            if(line->horizontal == AlignLeft) {
                x = model->x;
            } else if(line->horizontal == AlignCenter) {
                x = (model->x + model->width) / 2;
            } else if(line->horizontal == AlignRight) {
                x = model->x + model->width;
            }
            elements_draw_str_aligned_utf16(canvas, x, y, line->horizontal, AlignTop, line->text);
            y += font_info.px + 1;
        }
        // Draw scroll bar
        if(model->scroll_pos_total > 1) {
            elements_scrollbar_pos(
                canvas,
                model->x + model->width + WIDGET_ELEMENT_TEXT_SCROLL_BAR_OFFSET,
                model->y,
                model->height,
                model->scroll_pos_current,
                model->scroll_pos_total);
        }
    }

    furi_mutex_release(element->model_mutex);
}

static bool widget_element_text_scroll_input(InputEvent* event, WidgetElement* element) {
    furi_assert(event);
    furi_assert(element);

    furi_mutex_acquire(element->model_mutex, FuriWaitForever);

    WidgetElementTextScrollModel* model = element->model;
    bool consumed = false;

    if((event->type == InputTypeShort) || (event->type == InputTypeRepeat)) {
        if(event->key == InputKeyUp) {
            if(model->scroll_pos_current > 0) {
                model->scroll_pos_current--;
            }
            consumed = true;
        } else if(event->key == InputKeyDown) {
            if((model->scroll_pos_total > 1) &&
               (model->scroll_pos_current < model->scroll_pos_total - 1)) {
                model->scroll_pos_current++;
            }
            consumed = true;
        }
    }

    furi_mutex_release(element->model_mutex);

    return consumed;
}

static void widget_element_text_scroll_free(WidgetElement* text_scroll) {
    furi_assert(text_scroll);
    WidgetElementTextScrollModel* model = text_scroll->model;
    TextScrollLineArray_it_t it;
    for(TextScrollLineArray_it(it, model->line_array); !TextScrollLineArray_end_p(it);
        TextScrollLineArray_next(it)) {
        TextScrollLineArray* line = TextScrollLineArray_ref(it);
        uni_free(line->text);
    }
    TextScrollLineArray_clear(model->line_array);
    uni_free(model->text);
    free(text_scroll->model);
    furi_mutex_free(text_scroll->model_mutex);
    free(text_scroll);
}

WidgetElement* widget_element_text_scroll_create(
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    const char* text) {
    furi_assert(text);

    // Allocate and init model
    WidgetElementTextScrollModel* model = malloc(sizeof(WidgetElementTextScrollModel));
    model->x = x;
    model->y = y;
    model->width = width - WIDGET_ELEMENT_TEXT_SCROLL_BAR_OFFSET;
    model->height = height;
    model->scroll_pos_current = 0;
    model->scroll_pos_total = 1;
    TextScrollLineArray_init(model->line_array);
    model->text = uni_alloc_decode_utf8(text);

    WidgetElement* text_scroll = malloc(sizeof(WidgetElement));
    text_scroll->parent = NULL;
    text_scroll->draw = widget_element_text_scroll_draw;
    text_scroll->input = widget_element_text_scroll_input;
    text_scroll->free = widget_element_text_scroll_free;
    text_scroll->model = model;
    text_scroll->model_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    return text_scroll;
} //-V773
