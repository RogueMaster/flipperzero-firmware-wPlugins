#include "small_submenu.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FuriString* label;
    uint32_t index;
    SmallSubmenuItemCallback callback;
    void* callback_context;
} SmallSubmenuItem;

typedef struct {
    SmallSubmenuItem* items;
    size_t count;
    size_t capacity;
    size_t position;
    size_t window_position;
    FuriString* header;
} SmallSubmenuModel;

struct SmallSubmenu {
    View* view;
};

static void small_submenu_items_clear(SmallSubmenuModel* model) {
    if(!model) return;

    for(size_t i = 0; i < model->count; i++) {
        if(model->items[i].label) {
            furi_string_free(model->items[i].label);
            model->items[i].label = NULL;
        }
    }

    free(model->items);
    model->items = NULL;
    model->count = 0;
    model->capacity = 0;
    model->position = 0;
    model->window_position = 0;
}

static void small_submenu_draw(Canvas* canvas, void* context) {
    SmallSubmenuModel* model = context;

    const uint8_t header_height = 14;
    const uint8_t item_height = 12;
    const size_t items_on_screen = 4;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->header && !furi_string_empty(model->header)) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, furi_string_get_cstr(model->header));
    }

    // Compact system font with almost the full 128px line available.
    canvas_set_font(canvas, FontKeyboard);

    for(size_t slot = 0; slot < items_on_screen; slot++) {
        size_t item_index = model->window_position + slot;
        if(item_index >= model->count) break;

        int32_t y = header_height + (int32_t)(slot * item_height);

        if(item_index == model->position) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(canvas, 0, y, 128, item_height - 1);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_str(
            canvas, 1, y + item_height - 3, furi_string_get_cstr(model->items[item_index].label));
    }
}

static void small_submenu_up(SmallSubmenu* submenu) {
    with_view_model(
        submenu->view,
        SmallSubmenuModel * model,
        {
            const size_t items_on_screen = 4;

            if(model->count == 0) {
                model->position = 0;
                model->window_position = 0;
            } else if(model->position > 0) {
                model->position--;
                if(model->position < model->window_position) {
                    model->window_position = model->position;
                }
            } else {
                model->position = model->count - 1;
                model->window_position =
                    model->count > items_on_screen ? model->count - items_on_screen : 0;
            }
        },
        true);
}

static void small_submenu_down(SmallSubmenu* submenu) {
    with_view_model(
        submenu->view,
        SmallSubmenuModel * model,
        {
            const size_t items_on_screen = 4;

            if(model->count == 0) {
                model->position = 0;
                model->window_position = 0;
            } else if(model->position + 1 < model->count) {
                model->position++;
                if(model->position >= model->window_position + items_on_screen) {
                    model->window_position = model->position - items_on_screen + 1;
                }
            } else {
                model->position = 0;
                model->window_position = 0;
            }
        },
        true);
}

static void small_submenu_ok(SmallSubmenu* submenu) {
    SmallSubmenuItemCallback callback = NULL;
    void* callback_context = NULL;
    uint32_t index = 0;

    with_view_model(
        submenu->view,
        SmallSubmenuModel * model,
        {
            if(model->count > 0 && model->position < model->count) {
                SmallSubmenuItem* item = &model->items[model->position];
                callback = item->callback;
                callback_context = item->callback_context;
                index = item->index;
            }
        },
        false);

    if(callback) callback(callback_context, index);
}

static bool small_submenu_input(InputEvent* event, void* context) {
    SmallSubmenu* submenu = context;
    if(!submenu || !event) return false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        small_submenu_ok(submenu);
        return true;
    }

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            small_submenu_up(submenu);
            return true;
        } else if(event->key == InputKeyDown) {
            small_submenu_down(submenu);
            return true;
        }
    }

    return false;
}

SmallSubmenu* small_submenu_alloc(void) {
    SmallSubmenu* submenu = malloc(sizeof(SmallSubmenu));
    furi_check(submenu);

    submenu->view = view_alloc();
    furi_check(submenu->view);

    view_set_context(submenu->view, submenu);
    view_allocate_model(submenu->view, ViewModelTypeLocking, sizeof(SmallSubmenuModel));
    view_set_draw_callback(submenu->view, small_submenu_draw);
    view_set_input_callback(submenu->view, small_submenu_input);

    with_view_model(
        submenu->view,
        SmallSubmenuModel * model,
        {
            memset(model, 0, sizeof(SmallSubmenuModel));
            model->header = furi_string_alloc();
        },
        true);

    return submenu;
}

void small_submenu_free(SmallSubmenu* submenu) {
    if(!submenu) return;

    with_view_model(
        submenu->view,
        SmallSubmenuModel * model,
        {
            small_submenu_items_clear(model);
            if(model->header) {
                furi_string_free(model->header);
                model->header = NULL;
            }
        },
        true);

    view_free(submenu->view);
    submenu->view = NULL;
    free(submenu);
}

View* small_submenu_get_view(SmallSubmenu* submenu) {
    furi_check(submenu);
    return submenu->view;
}

void small_submenu_add_item(
    SmallSubmenu* submenu,
    const char* label,
    uint32_t index,
    SmallSubmenuItemCallback callback,
    void* callback_context) {
    furi_check(submenu);
    furi_check(label);

    with_view_model(
        submenu->view,
        SmallSubmenuModel * model,
        {
            if(model->count == model->capacity) {
                size_t new_capacity = model->capacity ? model->capacity * 2U : 8U;
                SmallSubmenuItem* new_items =
                    realloc(model->items, new_capacity * sizeof(SmallSubmenuItem));
                furi_check(new_items);

                model->items = new_items;
                memset(
                    &model->items[model->capacity],
                    0,
                    (new_capacity - model->capacity) * sizeof(SmallSubmenuItem));
                model->capacity = new_capacity;
            }

            SmallSubmenuItem* item = &model->items[model->count++];
            item->label = furi_string_alloc();
            furi_string_set_str(item->label, label);
            item->index = index;
            item->callback = callback;
            item->callback_context = callback_context;
        },
        true);
}

void small_submenu_reset(SmallSubmenu* submenu) {
    furi_check(submenu);

    with_view_model(
        submenu->view,
        SmallSubmenuModel * model,
        {
            small_submenu_items_clear(model);
            furi_string_reset(model->header);
        },
        true);
}

void small_submenu_set_header(SmallSubmenu* submenu, const char* header) {
    furi_check(submenu);

    with_view_model(
        submenu->view,
        SmallSubmenuModel * model,
        {
            if(header)
                furi_string_set_str(model->header, header);
            else
                furi_string_reset(model->header);
        },
        true);
}
