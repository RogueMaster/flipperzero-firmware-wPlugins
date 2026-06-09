#include <gui/view.h>
#include <gui/elements.h>
#include <gui/modules/submenu.h>
#include <notification/notification.h>

#include <stratahero_icons.h>

#include "constants.h"
#include "types.h"
#include "stratagems.h"
#include "catalog.h"
#include "glyphs.h"
#include "notifications.h"

#define LONG_TEXT_SCROLL_DELAY 3

struct StratagemTypesWidget {
    Submenu* menu;
    StratagemTypeSelectedCallback selected_callback;
    void* selected_callback_context;
};

static void stratagem_types_submenu_callback(void* context, uint32_t index) {
    StratagemTypesWidget* widget = context;
    if(widget->selected_callback) {
        widget->selected_callback((StratagemType)index, widget->selected_callback_context);
    }
}

StratagemTypesWidget* stratagem_types_widget_alloc() {
    StratagemTypesWidget* widget = malloc(sizeof(StratagemTypesWidget));
    widget->menu = submenu_alloc();
    for(int i = 0; i < StratagemTypeCount; i++) {
        submenu_add_item(
            widget->menu,
            get_stratagem_type_title((StratagemType)i),
            i,
            stratagem_types_submenu_callback,
            widget);
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
    void* context) {
    widget->selected_callback = callback;
    widget->selected_callback_context = context;
}

#define STRATAGEM_ITEM_HEIGHT 32
#define SCROLL_INTERVAL       (500)

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
    with_view_model(
        widget->view,
        StratagemListWidgetModel * model,
        { model->horizontal_scroll_counter++; },
        true);
}

static void stratagem_list_widget_view_draw_callback(Canvas* canvas, void* _model) {
    StratagemListWidgetModel* model = _model;

    canvas_clear(canvas);

    int visible_item_count = (SCREEN_HEIGHT + STRATAGEM_ITEM_HEIGHT - 1) / STRATAGEM_ITEM_HEIGHT;
    int last_visible_item = model->vertical_scroll_offset + visible_item_count;
    if(last_visible_item >= model->item_count) {
        last_visible_item = model->item_count - 1;
    }

    int stratagem_index = -1;
    for(int i = 0;
        (stratagem_index < (int)stratagems_count) && (i < model->vertical_scroll_offset);
        i++) {
        stratagem_index++;
        while(stratagems[stratagem_index]->type != model->stratagem_type) {
            stratagem_index++;
        }
    }

    int offset_y = 0;
    for(int i = model->vertical_scroll_offset; i <= last_visible_item;
        i++, offset_y += STRATAGEM_ITEM_HEIGHT) {
        stratagem_index++;
        while(stratagems[stratagem_index]->type != model->stratagem_type) {
            stratagem_index++;
        }
        if(stratagem_index >= (int)stratagems_count) {
            break;
        }

        if(i == model->selected_index) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, 0, offset_y, SCREEN_WIDTH - 5, STRATAGEM_ITEM_HEIGHT, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        Stratagem* stratagem = stratagems[stratagem_index];

        const Icon* icon = stratagem->icon;
        if(!icon) {
            icon = &I_no_icon_stratagem;
        }
        canvas_draw_icon(canvas, 4, offset_y + 4, icon);

        canvas_set_font(canvas, FontSecondary);
        FuriString* title = furi_string_alloc_set(stratagem->title);
        elements_scrollable_text_line(
            canvas,
            32,
            offset_y + 12,
            SCREEN_WIDTH - 32 - 10,
            title,
            (i == model->selected_index) ?
                (model->horizontal_scroll_counter < LONG_TEXT_SCROLL_DELAY ?
                     0 :
                     model->horizontal_scroll_counter - LONG_TEXT_SCROLL_DELAY) :
                0,
            false,
            false);

        furi_string_free(title);

        int code_glyph_offset = 32;
        for(int i = 0; stratagem->code[i]; i++) {
            const StrataHeroCodeGlyph* glyph = stratahero_get_code_glyph(stratagem->code[i]);
            if(glyph) {
                canvas_draw_icon(canvas, code_glyph_offset, offset_y + 16, glyph->black);
                code_glyph_offset += CODE_GLYPH_WIDTH;
            }
        }
    }

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, SCREEN_WIDTH - 5, 0, 5, SCREEN_HEIGHT);

    canvas_set_color(canvas, ColorBlack);
    for(int i = 2; i < SCREEN_HEIGHT - 2; i += 2) {
        canvas_draw_dot(canvas, SCREEN_WIDTH - 2, i);
    }

    int handle_height = (SCREEN_HEIGHT - 4) / model->item_count;
    if(handle_height < 4) {
        handle_height = 6;
    }
    int handle_y =
        2 + (SCREEN_HEIGHT - 4 - handle_height) * model->selected_index / (model->item_count - 1);
    canvas_draw_box(canvas, SCREEN_WIDTH - 3, handle_y, 3, handle_height);
}

static const Stratagem* stratagem_list_get_selected(StratagemListWidget* widget) {
    StratagemType type;
    int selected_index;
    with_view_model(
        widget->view,
        StratagemListWidgetModel * model,
        {
            type = model->stratagem_type;
            selected_index = model->selected_index;
        },
        false);

    int count = 0;
    for(uint32_t i = 0; i < stratagems_count; i++) {
        if(stratagems[i]->type == type) {
            if(count == selected_index) return stratagems[i];
            count++;
        }
    }
    return NULL;
}

static bool stratagem_list_widget_input_callback(InputEvent* event, void* context) {
    StratagemListWidget* widget = context;
    if(event->type != InputTypeShort) return false;

    bool handled = false;
    if(event->key == InputKeyOk && widget->selected_callback) {
        const Stratagem* stratagem = stratagem_list_get_selected(widget);
        if(stratagem) {
            widget->selected_callback(stratagem, widget->selected_callback_context);
        }
        handled = true;
    } else if(event->key == InputKeyDown) {
        with_view_model(
            widget->view,
            StratagemListWidgetModel * model,
            {
                if(model->selected_index + 1 < model->item_count) {
                    model->selected_index++;
                    if(model->selected_index - model->vertical_scroll_offset >= 2) {
                        model->vertical_scroll_offset++;
                    }
                } else {
                    model->selected_index = 0;
                    model->vertical_scroll_offset = 0;
                }
                model->horizontal_scroll_counter = 0;
            },
            true);
        handled = true;
    } else if(event->key == InputKeyUp) {
        with_view_model(
            widget->view,
            StratagemListWidgetModel * model,
            {
                if(model->selected_index > 0) {
                    model->selected_index--;
                    if(model->selected_index < model->vertical_scroll_offset) {
                        model->vertical_scroll_offset = model->selected_index;
                    }
                } else {
                    model->selected_index = model->item_count - 1;
                    model->vertical_scroll_offset =
                        (model->item_count > 1) ? model->item_count - 2 : 0;
                }
                model->horizontal_scroll_counter = 0;
            },
            true);
        handled = true;
    }

    return handled;
}

static void stratagem_list_widget_enter_callback(void* context) {
    StratagemListWidget* widget = context;
    with_view_model(
        widget->view,
        StratagemListWidgetModel * model,
        { furi_timer_start(model->scroll_timer, SCROLL_INTERVAL); },
        false);
}

static void stratagem_list_widget_exit_callback(void* context) {
    StratagemListWidget* widget = context;
    with_view_model(
        widget->view,
        StratagemListWidgetModel * model,
        { furi_timer_stop(model->scroll_timer); },
        false);
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

    with_view_model(
        widget->view,
        StratagemListWidgetModel * model,
        {
            model->selected_index = 0;
            model->vertical_scroll_offset = 0;
            model->scroll_timer =
                furi_timer_alloc(scroll_timer_callback, FuriTimerTypePeriodic, widget);
        },
        true);

    return widget;
}

View* stratagem_list_widget_get_view(StratagemListWidget* widget) {
    return widget->view;
}

void stratagem_list_widget_set_stratagem_type(StratagemListWidget* widget, StratagemType type) {
    with_view_model(
        widget->view,
        StratagemListWidgetModel * model,
        {
            model->stratagem_type = type;
            model->selected_index = 0;
            model->vertical_scroll_offset = 0;
            model->item_count = 0;
            for(int i = 0; i < (int)stratagems_count; i++) {
                if(stratagems[i]->type == type) {
                    model->item_count++;
                }
            }
        },
        true);
}

void stratagem_list_widget_free(StratagemListWidget* widget) {
    with_view_model(
        widget->view,
        StratagemListWidgetModel * model,
        { furi_timer_free(model->scroll_timer); },
        false);
    view_free(widget->view);
    free(widget);
}

void stratagem_list_widget_set_selected_callback(
    StratagemListWidget* widget,
    StratagemSelectedCallback callback,
    void* context) {
    widget->selected_callback = callback;
    widget->selected_callback_context = context;
}

// StratagemDetailWidget

struct StratagemDetailWidget {
    View* view;
    StratagemTrainCallback train_callback;
    void* train_callback_context;
};

typedef struct {
    const Stratagem* stratagem;
    int horizontal_scroll_counter;
    FuriTimer* scroll_timer;
} StratagemDetailWidgetModel;

static void detail_scroll_timer_callback(void* context) {
    StratagemDetailWidget* widget = context;
    with_view_model(
        widget->view,
        StratagemDetailWidgetModel * model,
        { model->horizontal_scroll_counter++; },
        true);
}

static void stratagem_detail_widget_enter_callback(void* context) {
    StratagemDetailWidget* widget = context;
    with_view_model(
        widget->view,
        StratagemDetailWidgetModel * model,
        {
            model->horizontal_scroll_counter = 0;
            furi_timer_start(model->scroll_timer, SCROLL_INTERVAL);
        },
        false);
}

static void stratagem_detail_widget_exit_callback(void* context) {
    StratagemDetailWidget* widget = context;
    with_view_model(
        widget->view,
        StratagemDetailWidgetModel * model,
        { furi_timer_stop(model->scroll_timer); },
        false);
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

    canvas_draw_icon(canvas, 4, 4, icon);

    int title_x = 4 + icon_w + 4;
    int title_w = SCREEN_WIDTH - title_x;
    int scroll_offset = model->horizontal_scroll_counter < LONG_TEXT_SCROLL_DELAY ?
                            0 :
                            model->horizontal_scroll_counter - LONG_TEXT_SCROLL_DELAY;
    FuriString* title = furi_string_alloc_set(stratagem->title);
    elements_scrollable_text_line(
        canvas, title_x, 12, title_w, title, scroll_offset, false, false);
    furi_string_free(title);

    int cooldown_x = title_x;
    int cooldown_y = 17;
    int cooldown_icon_w = icon_get_width(&I_cooldown);
    canvas_draw_icon(canvas, cooldown_x, cooldown_y, &I_cooldown);
    char buffer[32];
    snprintf(buffer, sizeof(buffer) - 1, "%ds", stratagem->cooldown);
    canvas_draw_str_aligned(
        canvas, cooldown_x + cooldown_icon_w + 2, cooldown_y + 2, AlignLeft, AlignTop, buffer);

    int code_len = strlen(stratagem->code);
    int code_x = (SCREEN_WIDTH - CODE_GLYPH_WIDTH * code_len) / 2;
    if(code_x < 4) code_x = 4;
    int code_y = 4 + icon_h + 4 + 2;
    for(int i = 0; i < code_len; i++) {
        const StrataHeroCodeGlyph* glyph = stratahero_get_code_glyph(stratagem->code[i]);
        if(glyph) {
            canvas_draw_icon(canvas, code_x, code_y, glyph->black);
        }
        code_x += CODE_GLYPH_WIDTH;
    }

    elements_button_right(canvas, "Train");
}

static bool stratagem_detail_widget_input_callback(InputEvent* event, void* context) {
    StratagemDetailWidget* widget = context;
    if(event->type != InputTypeShort) return false;
    if(event->key != InputKeyRight) return false;

    const Stratagem* stratagem = NULL;
    with_view_model(
        widget->view, StratagemDetailWidgetModel * model, { stratagem = model->stratagem; }, false);

    if(stratagem && widget->train_callback) {
        widget->train_callback(stratagem, widget->train_callback_context);
    }
    return true;
}

StratagemDetailWidget* stratagem_detail_widget_alloc() {
    StratagemDetailWidget* widget = malloc(sizeof(StratagemDetailWidget));
    widget->view = view_alloc();
    view_set_context(widget->view, widget);
    view_set_draw_callback(widget->view, stratagem_detail_widget_draw_callback);
    view_set_input_callback(widget->view, stratagem_detail_widget_input_callback);
    view_set_enter_callback(widget->view, stratagem_detail_widget_enter_callback);
    view_set_exit_callback(widget->view, stratagem_detail_widget_exit_callback);
    view_allocate_model(widget->view, ViewModelTypeLockFree, sizeof(StratagemDetailWidgetModel));
    widget->train_callback = NULL;
    widget->train_callback_context = NULL;
    with_view_model(
        widget->view,
        StratagemDetailWidgetModel * model,
        {
            model->horizontal_scroll_counter = 0;
            model->scroll_timer =
                furi_timer_alloc(detail_scroll_timer_callback, FuriTimerTypePeriodic, widget);
        },
        false);
    return widget;
}

void stratagem_detail_widget_set_train_callback(
    StratagemDetailWidget* widget,
    StratagemTrainCallback callback,
    void* context) {
    widget->train_callback = callback;
    widget->train_callback_context = context;
}

void stratagem_detail_widget_free(StratagemDetailWidget* widget) {
    with_view_model(
        widget->view,
        StratagemDetailWidgetModel * model,
        { furi_timer_free(model->scroll_timer); },
        false);
    view_free(widget->view);
    free(widget);
}

View* stratagem_detail_widget_get_view(StratagemDetailWidget* widget) {
    return widget->view;
}

void stratagem_detail_widget_set_stratagem(
    StratagemDetailWidget* widget,
    const Stratagem* stratagem) {
    with_view_model(
        widget->view,
        StratagemDetailWidgetModel * model,
        {
            model->stratagem = stratagem;
            model->horizontal_scroll_counter = 0;
        },
        true);
}

// StratagemTrainWidget

#define TRAIN_FLASH_DELAY 500

struct StratagemTrainWidget {
    View* view;
    FuriTimer* flash_timer;
    NotificationApp* notification;
    StrataHeroSettings settings;
};

typedef struct {
    const Stratagem* stratagem;
    int code_progress;
    bool input_blocked;
} StratagemTrainWidgetModel;

static void train_flash_timer_callback(void* context) {
    StratagemTrainWidget* widget = context;
    with_view_model(
        widget->view,
        StratagemTrainWidgetModel * model,
        {
            model->input_blocked = false;
            model->code_progress = 0;
        },
        true);
}

static void stratagem_train_widget_draw_callback(Canvas* canvas, void* _model) {
    StratagemTrainWidgetModel* model = _model;
    const Stratagem* stratagem = model->stratagem;

    canvas_clear(canvas);
    if(!stratagem) return;

    const Icon* icon = stratagem->icon ? stratagem->icon : &I_no_icon_stratagem;
    int icon_w = icon_get_width(icon);
    int icon_h = icon_get_height(icon);
    canvas_draw_icon(canvas, (SCREEN_WIDTH - icon_w) / 2, 4, icon);

    int code_len = strlen(stratagem->code);
    int code_x = (SCREEN_WIDTH - CODE_GLYPH_WIDTH * code_len) / 2;
    if(code_x < 4) code_x = 4;
    int code_y = 4 + icon_h + 6;

    bool inverse = model->input_blocked;

    for(int i = 0; i < code_len; i++) {
        const StrataHeroCodeGlyph* glyph = stratahero_get_code_glyph(stratagem->code[i]);
        if(glyph) {
            const Icon* glyph_icon;
            if(inverse) {
                glyph_icon = glyph->inverse;
            } else if(i < model->code_progress) {
                glyph_icon = glyph->black;
            } else {
                glyph_icon = glyph->white;
            }
            canvas_draw_icon(canvas, code_x, code_y, glyph_icon);
        }
        code_x += CODE_GLYPH_WIDTH;
        if(code_x > SCREEN_WIDTH - CODE_GLYPH_WIDTH) {
            code_y += CODE_GLYPH_HEIGHT;
            code_x = (SCREEN_WIDTH - CODE_GLYPH_WIDTH * (code_len - i - 1)) / 2;
            if(code_x < 4) code_x = 4;
        }
    }
}

static bool stratagem_train_widget_input_callback(InputEvent* event, void* context) {
    StratagemTrainWidget* widget = context;
    if(event->type != InputTypeShort) return false;

    char code_input = 0;
    switch(event->key) {
    case InputKeyLeft:
        code_input = 'L';
        break;
    case InputKeyRight:
        code_input = 'R';
        break;
    case InputKeyUp:
        code_input = 'U';
        break;
    case InputKeyDown:
        code_input = 'D';
        break;
    default:
        break;
    }

    if(code_input == 0) return false;

    bool success = false;
    bool code_complete = false;
    bool wrong = false;
    with_view_model(
        widget->view,
        StratagemTrainWidgetModel * model,
        {
            if(!model->input_blocked && model->stratagem) {
                if(model->stratagem->code[model->code_progress] == code_input) {
                    model->code_progress++;
                    success = true;
                    if(model->code_progress >= (int)strlen(model->stratagem->code)) {
                        model->code_progress = 0;
                        code_complete = true;
                    }
                } else {
                    model->input_blocked = true;
                    wrong = true;
                }
            }
        },
        true);

    if(code_complete) {
        stratahero_code_complete_notification(widget->notification, &widget->settings);
    } else if(success) {
        stratahero_code_glyph_entry_success_notification(widget->notification, &widget->settings);
    }
    if(wrong) {
        stratahero_code_glyph_entry_failure_notification(widget->notification, &widget->settings);
        furi_timer_start(widget->flash_timer, TRAIN_FLASH_DELAY);
    }

    return true;
}

static void stratagem_train_widget_exit_callback(void* context) {
    StratagemTrainWidget* widget = context;
    furi_timer_stop(widget->flash_timer);
    with_view_model(
        widget->view,
        StratagemTrainWidgetModel * model,
        {
            model->input_blocked = false;
            model->code_progress = 0;
        },
        true);
}

StratagemTrainWidget* stratagem_train_widget_alloc() {
    StratagemTrainWidget* widget = malloc(sizeof(StratagemTrainWidget));
    widget->view = view_alloc();
    view_set_context(widget->view, widget);
    view_set_draw_callback(widget->view, stratagem_train_widget_draw_callback);
    view_set_input_callback(widget->view, stratagem_train_widget_input_callback);
    view_set_exit_callback(widget->view, stratagem_train_widget_exit_callback);
    view_allocate_model(widget->view, ViewModelTypeLockFree, sizeof(StratagemTrainWidgetModel));
    widget->flash_timer = furi_timer_alloc(train_flash_timer_callback, FuriTimerTypeOnce, widget);
    widget->notification = furi_record_open(RECORD_NOTIFICATION);
    return widget;
}

void stratagem_train_widget_free(StratagemTrainWidget* widget) {
    furi_timer_stop(widget->flash_timer);
    furi_timer_free(widget->flash_timer);
    view_free(widget->view);
    furi_record_close(RECORD_NOTIFICATION);
    free(widget);
}

void stratagem_train_widget_set_settings(
    StratagemTrainWidget* widget,
    const StrataHeroSettings* settings) {
    widget->settings = *settings;
}

View* stratagem_train_widget_get_view(StratagemTrainWidget* widget) {
    return widget->view;
}

void stratagem_train_widget_set_stratagem(StratagemTrainWidget* widget, const Stratagem* stratagem) {
    with_view_model(
        widget->view,
        StratagemTrainWidgetModel * model,
        {
            model->stratagem = stratagem;
            model->code_progress = 0;
            model->input_blocked = false;
        },
        true);
}
