#include <gui/view.h>
#include <gui/elements.h>

#include <stratahero_icons.h>

#include "constants.h"
#include "types.h"
#include "stratagems.h"
#include "catalog.h"
#include "glyphs.h"


#define STRATAGEM_ITEM_HEIGHT 32
#define SCROLL_INTERVAL (500)


struct StratagemListWidget {
    View* view;
};

typedef struct {
    int selected_index;
    int vertical_scroll_offset;
    int horizontal_scroll_counter;

    StratagemType stratagem_type;
    int item_count;

    FuriTimer* scroll_timer;
} StratagemListWidgetModel;

static void scroll_timer_callback(void* context) {
    StratagemListWidget* widget = context;
    with_view_model(widget->view, StratagemListWidgetModel* model, {
        model->horizontal_scroll_counter++;
    }, true);
}

static void stratagem_list_widget_view_draw_callback(Canvas* canvas, void* _model) {
    StratagemListWidgetModel* model = _model;

    canvas_clear(canvas);

    int visible_item_count = (SCREEN_HEIGHT + STRATAGEM_ITEM_HEIGHT - 1) / STRATAGEM_ITEM_HEIGHT;
    int last_visible_item = model->vertical_scroll_offset + visible_item_count;
    if (last_visible_item >= model->item_count) {
        last_visible_item = model->item_count - 1;
    }

    int stratagem_index = -1;
    for (int i=0; (stratagem_index < (int)stratagems_count) && (i < model->vertical_scroll_offset); i++) {
        stratagem_index++;
        while (stratagems[stratagem_index]->type != model->stratagem_type) {
            stratagem_index++;
        }
    }

    int offset_y = 0;
    for (int i=model->vertical_scroll_offset; i <= last_visible_item; i++, offset_y += STRATAGEM_ITEM_HEIGHT) {
        stratagem_index++;
        while (stratagems[stratagem_index]->type != model->stratagem_type) {
            stratagem_index++;
        }
        if (stratagem_index >= (int)stratagems_count) {
            break;
        }

        if (i == model->selected_index) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, 0, offset_y, SCREEN_WIDTH - 5, STRATAGEM_ITEM_HEIGHT, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        Stratagem* stratagem = stratagems[stratagem_index];

        const Icon* icon = stratagem->icon;
        if (!icon) {
            icon = &I_no_icon_stratagem;
        }
        canvas_draw_icon(canvas, 4, offset_y + 4, icon);

        canvas_set_font(canvas, FontSecondary);
        FuriString* title = furi_string_alloc_set(stratagem->title);
        elements_scrollable_text_line(
            canvas, 32, offset_y + 12, SCREEN_WIDTH - 32 - 8,
            title,
            (i == model->selected_index) ? model->horizontal_scroll_counter : 0,
            false
        );
            // canvas_draw_str(canvas, 32, offset_y + 12, stratagem->title);
        furi_string_free(title);

        int code_glyph_offset = 32;
        for (int i=0; stratagem->code[i]; i++) {
            const StrataHeroCodeGlyph* glyph = stratahero_get_code_glyph(stratagem->code[i]);
            if (glyph) {
                canvas_draw_icon(canvas, code_glyph_offset, offset_y + 16, glyph->black);
                code_glyph_offset += CODE_GLYPH_WIDTH;
            }
        }
    }

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, SCREEN_WIDTH - 5, 0, 5, SCREEN_HEIGHT);

    canvas_set_color(canvas, ColorBlack);
    for (int i=2; i < SCREEN_HEIGHT - 2; i+=2) {
        canvas_draw_dot(canvas, SCREEN_WIDTH - 2, i);
    }

    int handle_height = (SCREEN_HEIGHT - 4) / model->item_count;
    if (handle_height < 4) {
        handle_height = 6;
    }
    int handle_y = 2 + (SCREEN_HEIGHT - 4 - handle_height) * model->selected_index / (model->item_count - 1);
    canvas_draw_box(canvas, SCREEN_WIDTH - 3, handle_y, 3, handle_height);
}

static bool stratagem_list_widget_input_callback(InputEvent* event, void* context) {
    StratagemListWidget* widget = context;
    if (event->type != InputTypeShort) return false;

    bool handled = false;
    if (event->key == InputKeyDown) {
        with_view_model(
            widget->view,
            StratagemListWidgetModel* model,
            {
                if (model->selected_index + 1 < model->item_count) {
                    model->selected_index++;
                    if (model->selected_index - model->vertical_scroll_offset >= 2) {
                        model->vertical_scroll_offset++;
                    }
                } else {
                    model->selected_index = 0;
                    model->vertical_scroll_offset = 0;
                }
                model->horizontal_scroll_counter = 0;
            },
            true
        );
        handled = true;
    } else if (event->key == InputKeyUp) {
        with_view_model(
            widget->view,
            StratagemListWidgetModel* model,
            {
                if (model->selected_index > 0) {
                    model->selected_index--;
                    if (model->selected_index < model->vertical_scroll_offset) {
                        model->vertical_scroll_offset = model->selected_index;
                    }
                } else {
                    model->selected_index = model->item_count - 1;
                    model->vertical_scroll_offset = (model->item_count > 1) ? model->item_count - 2 : 0;
                }
                model->horizontal_scroll_counter = 0;
            },
            true
        );
        handled = true;
    }

    return handled;
}

static void stratagem_list_widget_enter_callback(void* context) {
    StratagemListWidget* widget = context;
    with_view_model(widget->view, StratagemListWidgetModel* model, {
        furi_timer_start(model->scroll_timer, SCROLL_INTERVAL);
    }, false);
}

static void stratagem_list_widget_exit_callback(void* context) {
    StratagemListWidget* widget = context;
    with_view_model(widget->view, StratagemListWidgetModel* model, {
        furi_timer_stop(model->scroll_timer);
    }, false);
}

StratagemListWidget* stratagem_list_widget_alloc() {
    StratagemListWidget* widget = malloc(sizeof(StratagemListWidget));

    widget->view = view_alloc();
    view_set_context(widget->view, widget);
    view_set_draw_callback(widget->view, stratagem_list_widget_view_draw_callback);
    view_set_input_callback(widget->view, stratagem_list_widget_input_callback);
    view_allocate_model(widget->view, ViewModelTypeLockFree, sizeof(StratagemListWidgetModel));
    view_set_enter_callback(widget->view, stratagem_list_widget_enter_callback);
    view_set_exit_callback(widget->view, stratagem_list_widget_exit_callback);

    with_view_model(widget->view, StratagemListWidgetModel* model, {
        model->selected_index = 0;
        model->vertical_scroll_offset = 0;
        model->scroll_timer = furi_timer_alloc(scroll_timer_callback, FuriTimerTypePeriodic, widget);
    }, true);

    return widget;
}

View* stratagem_list_widget_get_view(StratagemListWidget* widget) {
    return widget->view;
}

void stratagem_list_widget_set_stratagem_type(StratagemListWidget* widget, StratagemType type) {
    with_view_model(
        widget->view,
        StratagemListWidgetModel* model,
        {
            model->stratagem_type = type;
            model->selected_index = 0;
            model->vertical_scroll_offset = 0;
            model->item_count = 0;
            for (int i=0; i < (int)stratagems_count; i++) {
                if (stratagems[i]->type == type) {
                    model->item_count++;
                }
            }
        },
        true
    );
}

void stratagem_list_widget_free(StratagemListWidget* widget) {
    with_view_model(widget->view, StratagemListWidgetModel* model, {
        furi_timer_free(model->scroll_timer);
    }, false);
    view_free(widget->view);
    free(widget);
}

