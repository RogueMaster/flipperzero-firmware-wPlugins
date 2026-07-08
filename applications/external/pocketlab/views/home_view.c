#include "home_view.h"

#include <furi.h>
#include <gui/elements.h>

#define HOME_HEADER_HEIGHT 16
#define HOME_ROW_HEIGHT    12
#define HOME_VISIBLE_ROWS  4

struct HomeView {
    View* view;
    HomeViewCallback callback;
    void* context;
};

typedef struct {
    const char* title;
    const char* items[HOME_VIEW_MAX_ITEMS];
    size_t count;
    size_t selected;
    size_t offset;
} HomeModel;

// The PocketLab mark (10x10): a chat bubble with a smiley. Matches the icon.
static void home_view_draw_logo(Canvas* canvas, uint8_t x, uint8_t y) {
    static const uint16_t rows[10] = {510, 513, 645, 513, 645, 633, 513, 510, 64, 128};
    for(uint8_t row = 0; row < 10; row++) {
        for(uint8_t col = 0; col < 10; col++) {
            if(rows[row] & (1u << col)) {
                canvas_draw_dot(canvas, x + col, y + row);
            }
        }
    }
}

static void home_view_draw_callback(Canvas* canvas, void* context) {
    HomeModel* model = context;
    canvas_clear(canvas);

    home_view_draw_logo(canvas, 2, 3);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 16, 13, model->title);

    canvas_set_font(canvas, FontSecondary);
    for(size_t row = 0; row < HOME_VISIBLE_ROWS; row++) {
        const size_t index = model->offset + row;
        if(index >= model->count) {
            break;
        }

        const uint8_t y = HOME_HEADER_HEIGHT + row * HOME_ROW_HEIGHT;
        if(index == model->selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y, 128, HOME_ROW_HEIGHT);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 6, y + HOME_ROW_HEIGHT - 3, model->items[index]);
        canvas_set_color(canvas, ColorBlack);
    }

    if(model->count > HOME_VISIBLE_ROWS) {
        elements_scrollbar(canvas, model->selected, model->count);
    }
}

static void home_view_scroll(HomeModel* model) {
    if(model->selected < model->offset) {
        model->offset = model->selected;
    } else if(model->selected >= model->offset + HOME_VISIBLE_ROWS) {
        model->offset = model->selected - HOME_VISIBLE_ROWS + 1;
    }
}

static bool home_view_input_callback(InputEvent* event, void* context) {
    HomeView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    bool selected = false;
    uint32_t selected_index = 0;

    with_view_model(
        instance->view,
        HomeModel * model,
        {
            if(event->key == InputKeyUp) {
                if(model->selected > 0) {
                    model->selected--;
                    home_view_scroll(model);
                }
                consumed = true;
            } else if(event->key == InputKeyDown) {
                if(model->selected + 1 < model->count) {
                    model->selected++;
                    home_view_scroll(model);
                }
                consumed = true;
            } else if(event->key == InputKeyOk) {
                selected = true;
                selected_index = model->selected;
                consumed = true;
            }
        },
        true);

    if(selected && instance->callback) {
        instance->callback(instance->context, selected_index);
    }

    return consumed;
}

HomeView* home_view_alloc(void) {
    HomeView* instance = malloc(sizeof(HomeView));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(HomeModel));
    view_set_draw_callback(instance->view, home_view_draw_callback);
    view_set_input_callback(instance->view, home_view_input_callback);

    return instance;
}

void home_view_free(HomeView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* home_view_get_view(HomeView* instance) {
    furi_assert(instance);
    return instance->view;
}

void home_view_set_callback(HomeView* instance, HomeViewCallback callback, void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void home_view_configure(
    HomeView* instance,
    const char* title,
    const char* const* items,
    size_t count,
    uint32_t selected) {
    furi_assert(instance);
    furi_check(count <= HOME_VIEW_MAX_ITEMS);

    with_view_model(
        instance->view,
        HomeModel * model,
        {
            model->title = title;
            model->count = count;
            for(size_t i = 0; i < count; i++) {
                model->items[i] = items[i];
            }
            model->selected = selected < count ? selected : 0;
            model->offset = 0;
            home_view_scroll(model);
        },
        true);
}

uint32_t home_view_get_selected(HomeView* instance) {
    furi_assert(instance);
    uint32_t selected = 0;
    with_view_model(instance->view, HomeModel * model, { selected = model->selected; }, false);
    return selected;
}
