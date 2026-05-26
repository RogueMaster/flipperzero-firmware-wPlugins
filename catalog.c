#include <gui/view.h>
#include <gui/elements.h>
#include <gui/modules/submenu.h>

#include <stratahero_icons.h>

#include "constants.h"
#include "types.h"
#include "stratagems.h"
#include "catalog.h"
#include "glyphs.h"


#define LONG_TEXT_SCROLL_DELAY 3


struct StratagemTypesWidget {
    Submenu* menu;
    StratagemTypeSelectedCallback selected_callback;
    void* selected_callback_context;
};

static void stratagem_types_submenu_callback(void* context, uint32_t index) {
    StratagemTypesWidget* widget = context;
    if (widget->selected_callback) {
        widget->selected_callback((StratagemType)index, widget->selected_callback_context);
    }
}

StratagemTypesWidget* stratagem_types_widget_alloc() {
    StratagemTypesWidget* widget = malloc(sizeof(StratagemTypesWidget));
    widget->menu = submenu_alloc();
    for (int i = 0; i < StratagemTypeCount; i++) {
        submenu_add_item(widget->menu, get_stratagem_type_title((StratagemType)i), i, stratagem_types_submenu_callback, widget);
    }
    widget->selected_callback = NULL;
    widget->selected_callback_context = NULL;
    return widget;
}

void stratagem_types_widget_free(StratagemTypesWidget* widget) {
    submenu_free(widget->menu);
    free(widget);
}

View* stratagem_types_widget_get_view(StratagemTypesWidget* widget) {
    return submenu_get_view(widget->menu);
}

void stratagem_types_widget_set_selected_callback(
    StratagemTypesWidget* widget,
    StratagemTypeSelectedCallback callback,
    void* context
) {
    widget->selected_callback = callback;
    widget->selected_callback_context = context;
}


#define STRATAGEM_ITEM_HEIGHT 32
#define SCROLL_INTERVAL (500)


struct StratagemListWidget {
    View* view;
    StratagemSelectedCallback selected_callback;
    void* selected_callback_context;
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
            canvas, 32, offset_y + 12, SCREEN_WIDTH - 32 - 10,
            title,
            (i == model->selected_index)
                ? (model->horizontal_scroll_counter < LONG_TEXT_SCROLL_DELAY ? 0 : model->horizontal_scroll_counter - LONG_TEXT_SCROLL_DELAY)
                : 0,
            false
        );

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

static const Stratagem* stratagem_list_get_selected(StratagemListWidget* widget) {
    StratagemType type;
    int selected_index;
    with_view_model(widget->view, StratagemListWidgetModel* model, {
        type = model->stratagem_type;
        selected_index = model->selected_index;
    }, false);

    int count = 0;
    for (uint32_t i = 0; i < stratagems_count; i++) {
        if (stratagems[i]->type == type) {
            if (count == selected_index) return stratagems[i];
            count++;
        }
    }
    return NULL;
}

static bool stratagem_list_widget_input_callback(InputEvent* event, void* context) {
    StratagemListWidget* widget = context;
    if (event->type != InputTypeShort) return false;

    bool handled = false;
    if (event->key == InputKeyOk && widget->selected_callback) {
        const Stratagem* stratagem = stratagem_list_get_selected(widget);
        if (stratagem) {
            widget->selected_callback(stratagem, widget->selected_callback_context);
        }
        handled = true;
    } else if (event->key == InputKeyDown) {
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

    widget->selected_callback = NULL;
    widget->selected_callback_context = NULL;

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

void stratagem_list_widget_set_selected_callback(
    StratagemListWidget* widget,
    StratagemSelectedCallback callback,
    void* context
) {
    widget->selected_callback = callback;
    widget->selected_callback_context = context;
}


// StratagemDetailWidget

struct StratagemDetailWidget {
    View* view;
};

typedef struct {
    const Stratagem* stratagem;
} StratagemDetailWidgetModel;

static int detail_draw_title_wrapped(Canvas* canvas, int x, int y, int max_width, const char* str) {
    const int line_h = 9;
    char line[64] = "";
    int cur_y = y;
    const char* p = str;

    while(p && *p) {
        const char* space = p;
        while(*space && *space != ' ') space++;

        char word[32];
        int word_len = space - p;
        if(word_len >= (int)sizeof(word)) word_len = (int)sizeof(word) - 1;
        memcpy(word, p, word_len);
        word[word_len] = '\0';

        char candidate[128];
        if(line[0]) {
            snprintf(candidate, sizeof(candidate), "%s %s", line, word);
        } else {
            strncpy(candidate, word, sizeof(candidate) - 1);
            candidate[sizeof(candidate) - 1] = '\0';
        }

        if(!line[0] || canvas_string_width(canvas, candidate) <= (size_t)max_width) {
            strncpy(line, candidate, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
        } else {
            canvas_draw_str_aligned(canvas, x, cur_y, AlignLeft, AlignTop, line);
            cur_y += line_h;
            strncpy(line, word, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
        }

        p = *space ? space + 1 : NULL;
    }

    if(line[0]) {
        canvas_draw_str_aligned(canvas, x, cur_y, AlignLeft, AlignTop, line);
        cur_y += line_h;
    }

    return cur_y;
}

static void stratagem_detail_widget_draw_callback(Canvas* canvas, void* _model) {
    StratagemDetailWidgetModel* model = _model;
    const Stratagem* stratagem = model->stratagem;

    canvas_clear(canvas);
    if(!stratagem) return;

    canvas_set_font(canvas, FontSecondary);

    const Icon* icon = stratagem->icon ? stratagem->icon : &I_no_icon_stratagem;
    int icon_w = icon_get_width(icon);
    int icon_h = icon_get_height(icon);

    int icon_x = 4;
    int icon_y = 4;
    canvas_draw_icon(canvas, icon_x, icon_y, icon);

    int title_x = icon_x + icon_w + 4;
    // int title_bottom = detail_draw_title_wrapped(
    //     canvas, title_x, icon_y, SCREEN_WIDTH - title_x - 2, stratagem->title);
    detail_draw_title_wrapped(canvas, title_x, icon_y, SCREEN_WIDTH - title_x - 2, stratagem->title);

    int cooldown_x = 4;
    int cooldown_y = 4 + icon_h + 2;
    int cooldown_icon_w = icon_get_width(&I_cooldown);
    int cooldown_icon_h = icon_get_height(&I_cooldown);
    canvas_draw_icon(canvas, cooldown_x, cooldown_y, &I_cooldown);
    char buffer[32];
    snprintf(buffer, sizeof(buffer)-1, "%ds", stratagem->cooldown);
    canvas_draw_str_aligned(canvas, cooldown_x + cooldown_icon_w + 2, cooldown_y + 2, AlignLeft, AlignTop, buffer);

    int code_y = cooldown_y + cooldown_icon_h + 2;
    int code_len = strlen(stratagem->code);
    int code_x = 4;
    for(int i = 0; i < code_len; i++) {
        const StrataHeroCodeGlyph* glyph = stratahero_get_code_glyph(stratagem->code[i]);
        if(glyph) {
            canvas_draw_icon(canvas, code_x, code_y, glyph->black);
        }
        code_x += CODE_GLYPH_WIDTH;
    }
}

StratagemDetailWidget* stratagem_detail_widget_alloc() {
    StratagemDetailWidget* widget = malloc(sizeof(StratagemDetailWidget));
    widget->view = view_alloc();
    view_set_context(widget->view, widget);
    view_set_draw_callback(widget->view, stratagem_detail_widget_draw_callback);
    view_allocate_model(widget->view, ViewModelTypeLockFree, sizeof(StratagemDetailWidgetModel));
    return widget;
}

void stratagem_detail_widget_free(StratagemDetailWidget* widget) {
    view_free(widget->view);
    free(widget);
}

View* stratagem_detail_widget_get_view(StratagemDetailWidget* widget) {
    return widget->view;
}

void stratagem_detail_widget_set_stratagem(StratagemDetailWidget* widget, const Stratagem* stratagem) {
    with_view_model(widget->view, StratagemDetailWidgetModel* model, {
        model->stratagem = stratagem;
    }, true);
}

