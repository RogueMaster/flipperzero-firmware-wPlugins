#include "labs_list_view.h"

#include <furi.h>
#include <gui/elements.h>

#include "../helpers/pocketlab_content.h"

#define LABS_ROW_HEIGHT   16
#define LABS_VISIBLE_ROWS 4

struct LabsListView {
    View* view;
    LabsListViewCallback callback;
    void* context;
};

typedef struct {
    uint32_t completed_mask;
    size_t selected;
    size_t offset;
} LabsListModel;

static void labs_list_view_draw_check(Canvas* canvas, uint8_t cx, uint8_t cy) {
    canvas_draw_line(canvas, cx - 2, cy, cx - 1, cy + 2);
    canvas_draw_line(canvas, cx - 1, cy + 2, cx + 2, cy - 2);
}

// Draws the completion indicator so it stays readable in every state.
// The foreground color is white on a selected (inverted) row, black otherwise.
static void
    labs_list_view_draw_indicator(Canvas* canvas, uint8_t cx, uint8_t cy, bool done, bool selected) {
    const Color foreground = selected ? ColorWhite : ColorBlack;
    const Color background = selected ? ColorBlack : ColorWhite;

    if(!done) {
        canvas_set_color(canvas, foreground);
        canvas_draw_circle(canvas, cx, cy, 4);
    } else if(selected) {
        // Inverted outline plus an inverted check, no fill.
        canvas_set_color(canvas, foreground);
        canvas_draw_circle(canvas, cx, cy, 4);
        labs_list_view_draw_check(canvas, cx, cy);
    } else {
        // Filled disc with the check punched out in the background color.
        canvas_set_color(canvas, foreground);
        canvas_draw_disc(canvas, cx, cy, 4);
        canvas_set_color(canvas, background);
        labs_list_view_draw_check(canvas, cx, cy);
    }

    canvas_set_color(canvas, foreground);
}

static void labs_list_view_draw_callback(Canvas* canvas, void* context) {
    LabsListModel* model = context;
    canvas_clear(canvas);

    for(size_t row = 0; row < LABS_VISIBLE_ROWS; row++) {
        const size_t index = model->offset + row;
        if(index >= pocketlab_labs_count) {
            break;
        }

        const uint8_t y = row * LABS_ROW_HEIGHT;
        const bool selected = index == model->selected;
        const bool done = (model->completed_mask & (1UL << index)) != 0;

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y, 128, LABS_ROW_HEIGHT);
        }

        labs_list_view_draw_indicator(canvas, 10, y + LABS_ROW_HEIGHT / 2, done, selected);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 22, y + LABS_ROW_HEIGHT / 2 + 4, pocketlab_labs[index].title);

        canvas_set_color(canvas, ColorBlack);
    }

    elements_scrollbar(canvas, model->selected, pocketlab_labs_count);
}

static void labs_list_view_scroll(LabsListModel* model) {
    if(model->selected < model->offset) {
        model->offset = model->selected;
    } else if(model->selected >= model->offset + LABS_VISIBLE_ROWS) {
        model->offset = model->selected - LABS_VISIBLE_ROWS + 1;
    }
}

static bool labs_list_view_input_callback(InputEvent* event, void* context) {
    LabsListView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    bool selected = false;
    uint32_t selected_index = 0;

    with_view_model(
        instance->view,
        LabsListModel * model,
        {
            if(event->key == InputKeyUp) {
                if(model->selected > 0) {
                    model->selected--;
                    labs_list_view_scroll(model);
                }
                consumed = true;
            } else if(event->key == InputKeyDown) {
                if(model->selected + 1 < pocketlab_labs_count) {
                    model->selected++;
                    labs_list_view_scroll(model);
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

LabsListView* labs_list_view_alloc(void) {
    LabsListView* instance = malloc(sizeof(LabsListView));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(LabsListModel));
    view_set_draw_callback(instance->view, labs_list_view_draw_callback);
    view_set_input_callback(instance->view, labs_list_view_input_callback);

    return instance;
}

void labs_list_view_free(LabsListView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* labs_list_view_get_view(LabsListView* instance) {
    furi_assert(instance);
    return instance->view;
}

void labs_list_view_set_callback(
    LabsListView* instance,
    LabsListViewCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void labs_list_view_configure(LabsListView* instance, uint32_t completed_mask, uint32_t selected) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        LabsListModel * model,
        {
            model->completed_mask = completed_mask;
            model->selected = selected < pocketlab_labs_count ? selected : 0;
            model->offset = 0;
            labs_list_view_scroll(model);
        },
        true);
}

uint32_t labs_list_view_get_selected(LabsListView* instance) {
    furi_assert(instance);
    uint32_t selected = 0;
    with_view_model(instance->view, LabsListModel * model, { selected = model->selected; }, false);
    return selected;
}
