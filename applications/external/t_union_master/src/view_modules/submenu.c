#include "submenu.h"
#include "../font_provider/c_font.h"

#include <t_union_master_icons.h>
#include "elements.h"
#include <furi.h>
#include <m-array.h>

struct Submenu {
    View* view;
    FuriTimer* locked_timer;
};

typedef struct {
    FuriString* label;
    uint32_t index;
    SubmenuItemCallback callback;
    void* callback_context;
    bool locked;
    FuriString* locked_message;
} SubmenuItem;

static void SubmenuItem_init(SubmenuItem* item) {
    item->label = furi_string_alloc();
    item->index = 0;
    item->callback = NULL;
    item->callback_context = NULL;
    item->locked = false;
    item->locked_message = furi_string_alloc();
}

static void SubmenuItem_init_set(SubmenuItem* item, const SubmenuItem* src) {
    item->label = furi_string_alloc_set(src->label);
    item->index = src->index;
    item->callback = src->callback;
    item->callback_context = src->callback_context;
    item->locked = src->locked;
    item->locked_message = furi_string_alloc_set(src->locked_message);
}

static void SubmenuItem_set(SubmenuItem* item, const SubmenuItem* src) {
    furi_string_set(item->label, src->label);
    item->index = src->index;
    item->callback = src->callback;
    item->callback_context = src->callback_context;
    item->locked = src->locked;
    furi_string_set(item->locked_message, src->locked_message);
}

static void SubmenuItem_clear(SubmenuItem* item) {
    furi_string_free(item->label);
    furi_string_free(item->locked_message);
}

ARRAY_DEF(
    SubmenuItemArray,
    SubmenuItem,
    (INIT(API_2(SubmenuItem_init)),
     SET(API_6(SubmenuItem_set)),
     INIT_SET(API_6(SubmenuItem_init_set)),
     CLEAR(API_2(SubmenuItem_clear))))

typedef struct {
    SubmenuItemArray_t items;
    FuriString* header;
    size_t position;
    size_t window_position;
    bool locked_message_visible;
    bool is_vertical;
} SubmenuModel;

static void submenu_process_up(Submenu* submenu);
static void submenu_process_down(Submenu* submenu);
static void submenu_process_ok(Submenu* submenu);

static size_t submenu_items_on_screen(bool header, bool vertical) {
    size_t res = (vertical) ? 8 : 4;
    return (header) ? res - 1 : res;
}

static void submenu_view_draw_callback(Canvas* canvas, void* _model) {
    SubmenuModel* model = _model;

    const uint8_t item_height = 16;
    uint8_t item_width = canvas_width(canvas) - 5;

    canvas_clear(canvas);

    if(!furi_string_empty(model->header)) {
        // canvas_set_font(canvas, FontPrimary);
        elements_draw_str_utf8(canvas, 4, 11, furi_string_get_cstr(model->header));
    }

    // canvas_set_font(canvas, FontSecondary);

    size_t position = 0;
    SubmenuItemArray_it_t it;
    for(SubmenuItemArray_it(it, model->items); !SubmenuItemArray_end_p(it);
        SubmenuItemArray_next(it)) {
        const size_t item_position = position - model->window_position;
        const size_t items_on_screen =
            submenu_items_on_screen(!furi_string_empty(model->header), model->is_vertical);
        uint8_t y_offset = furi_string_empty(model->header) ? 0 : item_height;

        if(item_position < items_on_screen) {
            if(position == model->position) {
                canvas_set_color(canvas, ColorBlack);
                elements_slightly_rounded_box(
                    canvas,
                    0,
                    y_offset + (item_position * item_height) + 1,
                    item_width,
                    item_height - 2);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }

            if(SubmenuItemArray_cref(it)->locked) {
                canvas_draw_icon(
                    canvas,
                    item_width - 10,
                    y_offset + (item_position * item_height) + item_height - 12,
                    &I_Lock_7x8);
            }

            elements_draw_str_utf8(
                canvas,
                6,
                y_offset + (item_position * item_height) + item_height - 2,
                furi_string_get_cstr(SubmenuItemArray_cref(it)->label));
        }

        position++;
    }

    elements_scrollbar(canvas, model->position, SubmenuItemArray_size(model->items));

    if(model->locked_message_visible) {
        const uint8_t frame_x = 7;
        const uint8_t frame_width = canvas_width(canvas) - frame_x * 2;
        const uint8_t frame_y = 7;
        const uint8_t frame_height = canvas_height(canvas) - frame_y * 2;

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, frame_x + 2, frame_y + 2, frame_width - 4, frame_height - 4);

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_icon(
            canvas, frame_x + 2, canvas_height(canvas) - frame_y - 2 - 42, &I_WarningDolphin_45x42);

        canvas_draw_rframe(canvas, frame_x, frame_y, frame_width, frame_height, 3);
        canvas_draw_rframe(canvas, frame_x + 1, frame_y + 1, frame_width - 2, frame_height - 2, 2);
        if(model->is_vertical) {
            elements_multiline_text_aligned_utf8(
                canvas,
                32,
                42,
                AlignCenter,
                AlignCenter,
                furi_string_get_cstr(
                    SubmenuItemArray_get(model->items, model->position)->locked_message));
        } else {
            elements_multiline_text_aligned_utf8(
                canvas,
                84,
                32,
                AlignCenter,
                AlignCenter,
                furi_string_get_cstr(
                    SubmenuItemArray_get(model->items, model->position)->locked_message));
        }
    }
}

static bool submenu_view_input_callback(InputEvent* event, void* context) {
    Submenu* submenu = context;
    furi_assert(submenu);
    bool consumed = false;

    bool locked_message_visible = false;
    with_view_model(
        submenu->view,
        SubmenuModel * model,
        { locked_message_visible = model->locked_message_visible; },
        false);

    if((!(event->type == InputTypePress) && !(event->type == InputTypeRelease)) &&
       locked_message_visible) {
        with_view_model(
            submenu->view, SubmenuModel * model, { model->locked_message_visible = false; }, true);
        consumed = true;
    } else if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
            consumed = true;
            submenu_process_up(submenu);
            break;
        case InputKeyDown:
            consumed = true;
            submenu_process_down(submenu);
            break;
        case InputKeyOk:
            consumed = true;
            submenu_process_ok(submenu);
            break;
        default:
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            consumed = true;
            submenu_process_up(submenu);
        } else if(event->key == InputKeyDown) {
            consumed = true;
            submenu_process_down(submenu);
        }
    }

    return consumed;
}

void submenu_timer_callback(void* context) {
    furi_assert(context);
    Submenu* submenu = context;

    with_view_model(
        submenu->view, SubmenuModel * model, { model->locked_message_visible = false; }, true);
}

Submenu* submenu_alloc_utf8() {
    Submenu* submenu = malloc(sizeof(Submenu));
    submenu->view = view_alloc();
    view_set_context(submenu->view, submenu);
    view_allocate_model(submenu->view, ViewModelTypeLocking, sizeof(SubmenuModel));
    view_set_draw_callback(submenu->view, submenu_view_draw_callback);
    view_set_input_callback(submenu->view, submenu_view_input_callback);

    submenu->locked_timer = furi_timer_alloc(submenu_timer_callback, FuriTimerTypeOnce, submenu);

    with_view_model(
        submenu->view,
        SubmenuModel * model,
        {
            SubmenuItemArray_init(model->items);
            model->position = 0;
            model->window_position = 0;
            model->header = furi_string_alloc();
        },
        true);

    return submenu;
}

void submenu_process_up(Submenu* submenu) {
    with_view_model(
        submenu->view,
        SubmenuModel * model,
        {
            const size_t items_on_screen =
                submenu_items_on_screen(!furi_string_empty(model->header), model->is_vertical);
            const size_t items_size = SubmenuItemArray_size(model->items);

            if(model->position > 0) {
                model->position--;
                if((model->position == model->window_position) && (model->window_position > 0)) {
                    model->window_position--;
                }
            } else {
                model->position = items_size - 1;
                if(model->position > items_on_screen - 1) {
                    model->window_position = model->position - (items_on_screen - 1);
                }
            }
        },
        true);
}

void submenu_process_down(Submenu* submenu) {
    with_view_model(
        submenu->view,
        SubmenuModel * model,
        {
            const size_t items_on_screen =
                submenu_items_on_screen(!furi_string_empty(model->header), model->is_vertical);
            const size_t items_size = SubmenuItemArray_size(model->items);

            if(model->position < items_size - 1) {
                model->position++;
                if((model->position - model->window_position > items_on_screen - 2) &&
                   (model->window_position < items_size - items_on_screen)) {
                    model->window_position++;
                }
            } else {
                model->position = 0;
                model->window_position = 0;
            }
        },
        true);
}

void submenu_process_ok(Submenu* submenu) {
    SubmenuItem* item = NULL;

    with_view_model(
        submenu->view,
        SubmenuModel * model,
        {
            const size_t items_size = SubmenuItemArray_size(model->items);
            if(model->position < items_size) {
                item = SubmenuItemArray_get(model->items, model->position);
            }
            if(item && item->locked) {
                model->locked_message_visible = true;
                furi_timer_start(submenu->locked_timer, furi_kernel_get_tick_frequency() * 3);
            }
        },
        true);

    if(item && !item->locked && item->callback) {
        item->callback(item->callback_context, item->index);
    }
}
