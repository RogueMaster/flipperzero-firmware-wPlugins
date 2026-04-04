#include "flippass_db_browser_view.h"
#include "flippass_icons.h"

#include <furi.h>
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FLIPPASS_DB_BROWSER_SCROLL_INTERVAL_MS 333U
#define FLIPPASS_DB_BROWSER_VISIBLE_ROWS       4U
#define FLIPPASS_DB_BROWSER_ROW_HEIGHT         11U
#define FLIPPASS_DB_BROWSER_LIST_Y             9U
#define FLIPPASS_DB_BROWSER_ICON_X             4U
#define FLIPPASS_DB_BROWSER_TEXT_X             17U

typedef struct {
    char label[FLIPPASS_DB_BROWSER_LABEL_SIZE];
    FlipPassDbBrowserItemType type;
} FlipPassDbBrowserItem;

typedef struct {
    char header[FLIPPASS_DB_BROWSER_HEADER_SIZE];
    FlipPassDbBrowserItem items[FLIPPASS_DB_BROWSER_MAX_ITEMS];
    uint32_t item_count;
    uint32_t selected_index;
    uint32_t action_selected_index;
    size_t scroll_counter;
    bool has_parent;
    bool action_menu_open;
    FlipPassDbBrowserMode mode;
    FuriString* draw_string;
} FlipPassDbBrowserViewModel;

struct FlipPassDbBrowserView {
    View* view;
    FuriTimer* scroll_timer;
    FlipPassDbBrowserViewCallback callback;
    void* callback_context;
};

static const char* flippass_db_browser_action_labels[FlipPassDbBrowserActionCount] = {
    "AutoType",
    "Password",
    "User",
    "Other",
};

static uint32_t flippass_db_browser_clamp_action_index(uint32_t index) {
    return (index < FlipPassDbBrowserActionCount) ? index : 0U;
}

static bool flippass_db_browser_selected_is_group(const FlipPassDbBrowserViewModel* model) {
    return model->item_count > 0U && model->selected_index < model->item_count &&
           model->items[model->selected_index].type == FlipPassDbBrowserItemTypeGroup;
}

static bool flippass_db_browser_selected_is_entry(const FlipPassDbBrowserViewModel* model) {
    return model->item_count > 0U && model->selected_index < model->item_count &&
           model->items[model->selected_index].type == FlipPassDbBrowserItemTypeEntry;
}

static bool flippass_db_browser_selected_action_is_other(const FlipPassDbBrowserViewModel* model) {
    return model != NULL &&
           flippass_db_browser_clamp_action_index(model->action_selected_index) ==
               FlipPassDbBrowserActionOther;
}

static bool flippass_db_browser_selected_is_actionable(const FlipPassDbBrowserViewModel* model) {
    return model->item_count > 0U && model->selected_index < model->item_count &&
           model->items[model->selected_index].type != FlipPassDbBrowserItemTypeInfo;
}

static void flippass_db_browser_view_draw_item_icon(
    Canvas* canvas,
    FlipPassDbBrowserItemType type,
    uint8_t x,
    uint8_t y) {
    switch(type) {
    case FlipPassDbBrowserItemTypeGroup:
        canvas_draw_icon(canvas, x, y, &I_dir_10px);
        break;
    case FlipPassDbBrowserItemTypeEntry:
        canvas_draw_icon(canvas, x, y, &I_C00_Password);
        break;
    case FlipPassDbBrowserItemTypeField:
    case FlipPassDbBrowserItemTypeInfo:
    default:
        break;
    }
}

static void flippass_db_browser_view_draw_list(Canvas* canvas, FlipPassDbBrowserViewModel* model) {
    const bool show_scrollbar = model->item_count > FLIPPASS_DB_BROWSER_VISIBLE_ROWS;
    const uint32_t selected = (model->item_count > 0U) ? model->selected_index : 0U;
    uint32_t window_offset = 0U;

    if(model->item_count > FLIPPASS_DB_BROWSER_VISIBLE_ROWS) {
        const uint32_t max_offset = model->item_count - FLIPPASS_DB_BROWSER_VISIBLE_ROWS;
        if(selected >= FLIPPASS_DB_BROWSER_VISIBLE_ROWS) {
            window_offset = selected - (FLIPPASS_DB_BROWSER_VISIBLE_ROWS - 1U);
            if(window_offset > max_offset) {
                window_offset = max_offset;
            }
        }
    }

    canvas_set_font(canvas, FontSecondary);

    for(uint32_t row = 0U; row < FLIPPASS_DB_BROWSER_VISIBLE_ROWS; row++) {
        const uint32_t item_index = window_offset + row;
        const uint32_t y = FLIPPASS_DB_BROWSER_LIST_Y + row * FLIPPASS_DB_BROWSER_ROW_HEIGHT;
        const bool is_selected = item_index < model->item_count && item_index == selected;

        if(item_index >= model->item_count) {
            continue;
        }

        if(is_selected) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(
                canvas, 0, y, show_scrollbar ? 122U : 127U, FLIPPASS_DB_BROWSER_ROW_HEIGHT);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        furi_string_set_str(model->draw_string, model->items[item_index].label);
        const FlipPassDbBrowserItemType type = model->items[item_index].type;
        uint8_t label_x = 4U;
        uint8_t label_width = show_scrollbar ? 116U : 121U;

        if(type == FlipPassDbBrowserItemTypeGroup || type == FlipPassDbBrowserItemTypeEntry) {
            flippass_db_browser_view_draw_item_icon(
                canvas, type, FLIPPASS_DB_BROWSER_ICON_X, (uint8_t)(y + 1U));
            label_x = FLIPPASS_DB_BROWSER_TEXT_X;
            label_width = show_scrollbar ? 103U : 108U;
        }

        elements_scrollable_text_line(
            canvas,
            label_x,
            (uint8_t)(y + 9U),
            label_width,
            model->draw_string,
            is_selected ? model->scroll_counter : 0U,
            true);
    }

    canvas_set_color(canvas, ColorBlack);
    if(show_scrollbar) {
        elements_scrollbar_pos(
            canvas,
            126,
            FLIPPASS_DB_BROWSER_LIST_Y,
            FLIPPASS_DB_BROWSER_VISIBLE_ROWS * FLIPPASS_DB_BROWSER_ROW_HEIGHT - 1U,
            selected,
            model->item_count);
    }

    if(model->item_count == 0U) {
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "Empty");
    }
}

static void flippass_db_browser_view_draw_action_menu(
    Canvas* canvas,
    const FlipPassDbBrowserViewModel* model) {
    const uint8_t box_x = 60U;
    const uint8_t box_y = 0U;
    const uint8_t box_w = 68U;
    const uint8_t box_h = 50U;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, box_x + 1U, box_y + 1U, box_w - 2U, box_h - 2U);
    canvas_set_color(canvas, ColorBlack);
    elements_slightly_rounded_frame(canvas, box_x, box_y, box_w, box_h);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, box_x + (box_w / 2U), box_y + 10U, AlignCenter, AlignBottom, "Type");
    canvas_draw_line(canvas, box_x + 1U, box_y + 12U, box_x + box_w - 2U, box_y + 12U);

    canvas_set_font(canvas, FontSecondary);
    for(uint32_t index = 0U; index < FlipPassDbBrowserActionCount; index++) {
        const uint8_t row_y = (uint8_t)(15U + index * 9U);
        const bool selected = model->action_selected_index == index;

        if(selected) {
            canvas_draw_str(canvas, box_x + 4U, row_y + 7U, ">");
        }

        canvas_draw_str(canvas, box_x + 11U, row_y + 7U, flippass_db_browser_action_labels[index]);
    }

    if(flippass_db_browser_selected_action_is_other(model)) {
        elements_button_center(canvas, "Open");
    } else {
        elements_button_left(canvas, "BT");
        elements_button_center(canvas, "Show");
        elements_button_right(canvas, "USB");
    }
}

static void
    flippass_db_browser_view_draw_buttons(Canvas* canvas, const FlipPassDbBrowserViewModel* model) {
    if(model->action_menu_open) {
        flippass_db_browser_view_draw_action_menu(canvas, model);
        return;
    }

    if(model->mode == FlipPassDbBrowserModeDirectActions) {
        if(flippass_db_browser_selected_is_actionable(model)) {
            elements_button_left(canvas, "BT");
            elements_button_center(canvas, "Show");
            elements_button_right(canvas, "USB");
        }
        return;
    }

    if(model->has_parent) {
        elements_button_left(canvas, "Up");
    }

    if(flippass_db_browser_selected_is_entry(model)) {
        elements_button_center(canvas, "Type");
        return;
    }

    if(flippass_db_browser_selected_is_group(model)) {
        elements_button_right(canvas, "Enter");
        return;
    }

    if(model->has_parent) {
        elements_button_right(canvas, "Up");
    }
}

static void flippass_db_browser_view_draw_callback(Canvas* canvas, void* _model) {
    FlipPassDbBrowserViewModel* model = _model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    furi_string_set_str(model->draw_string, model->header);
    elements_scrollable_text_line(
        canvas, 2, 8, 124U, model->draw_string, model->scroll_counter, true);

    flippass_db_browser_view_draw_list(canvas, model);
    flippass_db_browser_view_draw_buttons(canvas, model);
}

static void
    flippass_db_browser_view_emit(FlipPassDbBrowserView* browser, FlipPassDbBrowserEvent event) {
    furi_assert(browser);

    if(browser->callback != NULL) {
        browser->callback(event, browser->callback_context);
    }
}

static void flippass_db_browser_view_move_selection(FlipPassDbBrowserView* browser, int8_t delta) {
    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        {
            if(model->action_menu_open) {
                const uint32_t action_count = FlipPassDbBrowserActionCount;
                const uint32_t current =
                    flippass_db_browser_clamp_action_index(model->action_selected_index);
                model->action_selected_index = (delta > 0) ?
                                                   ((current + 1U) % action_count) :
                                                   ((current + action_count - 1U) % action_count);
            } else if(model->item_count > 0U) {
                const uint32_t count = model->item_count;
                const uint32_t current = (model->selected_index < count) ? model->selected_index :
                                                                           0U;
                model->selected_index = (delta > 0) ? ((current + 1U) % count) :
                                                      ((current + count - 1U) % count);
            }
            model->scroll_counter = 0U;
        },
        true);
}

static bool flippass_db_browser_view_input_callback(InputEvent* event, void* context) {
    FlipPassDbBrowserView* browser = context;
    bool consumed = false;

    furi_assert(browser);
    furi_assert(event);

    if((event->key == InputKeyUp || event->key == InputKeyDown) &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        flippass_db_browser_view_move_selection(browser, event->key == InputKeyDown ? 1 : -1);
        return true;
    }

    if(event->type != InputTypeShort) {
        return false;
    }

    FlipPassDbBrowserViewModel* model = view_get_model(browser->view);
    const bool action_menu_open = model->action_menu_open;
    const bool selected_group = flippass_db_browser_selected_is_group(model);
    const bool selected_entry = flippass_db_browser_selected_is_entry(model);
    const bool selected_actionable = flippass_db_browser_selected_is_actionable(model);
    const bool has_parent = model->has_parent;
    const bool selected_other_action = flippass_db_browser_selected_action_is_other(model);
    const bool direct_actions_mode = model->mode == FlipPassDbBrowserModeDirectActions;
    view_commit_model(browser->view, false);

    if(action_menu_open) {
        switch(event->key) {
        case InputKeyLeft:
            if(!selected_other_action) {
                flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventTypeBluetooth);
                consumed = true;
            }
            break;
        case InputKeyOk:
            flippass_db_browser_view_emit(
                browser,
                selected_other_action ? FlipPassDbBrowserEventOpenOther :
                                        FlipPassDbBrowserEventShow);
            consumed = true;
            break;
        case InputKeyRight:
            if(!selected_other_action) {
                flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventTypeUsb);
                consumed = true;
            }
            break;
        case InputKeyBack:
            with_view_model(
                browser->view,
                FlipPassDbBrowserViewModel * mutable_model,
                { mutable_model->action_menu_open = false; },
                true);
            consumed = true;
            break;
        default:
            break;
        }
        return consumed;
    }

    if(direct_actions_mode) {
        switch(event->key) {
        case InputKeyLeft:
            if(selected_actionable) {
                flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventTypeBluetooth);
                consumed = true;
            }
            break;
        case InputKeyOk:
            if(selected_actionable) {
                flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventShow);
                consumed = true;
            }
            break;
        case InputKeyRight:
            if(selected_actionable) {
                flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventTypeUsb);
                consumed = true;
            }
            break;
        default:
            break;
        }

        return consumed;
    }

    switch(event->key) {
    case InputKeyLeft:
        if(has_parent) {
            flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventBack);
            consumed = true;
        }
        break;
    case InputKeyOk:
        if(selected_entry) {
            with_view_model(
                browser->view,
                FlipPassDbBrowserViewModel * mutable_model,
                { mutable_model->action_menu_open = true; },
                true);
            flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventOpenActionMenu);
            consumed = true;
        } else if(selected_group) {
            flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventEnter);
            consumed = true;
        }
        break;
    case InputKeyRight:
        if(selected_group) {
            flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventEnter);
            consumed = true;
        } else if(!selected_entry && has_parent) {
            flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventBack);
            consumed = true;
        }
        break;
    case InputKeyBack:
        if(has_parent) {
            flippass_db_browser_view_emit(browser, FlipPassDbBrowserEventBack);
            consumed = true;
        }
        break;
    default:
        break;
    }

    return consumed;
}

static void flippass_db_browser_view_timer_callback(void* context) {
    FlipPassDbBrowserView* browser = context;
    furi_assert(browser);

    with_view_model(
        browser->view, FlipPassDbBrowserViewModel * model, { model->scroll_counter++; }, true);
}

static void flippass_db_browser_view_enter_callback(void* context) {
    FlipPassDbBrowserView* browser = context;
    furi_assert(browser);

    with_view_model(
        browser->view, FlipPassDbBrowserViewModel * model, { model->scroll_counter = 0U; }, true);
    furi_timer_start(browser->scroll_timer, FLIPPASS_DB_BROWSER_SCROLL_INTERVAL_MS);
}

static void flippass_db_browser_view_exit_callback(void* context) {
    FlipPassDbBrowserView* browser = context;
    furi_assert(browser);
    furi_timer_stop(browser->scroll_timer);
}

FlipPassDbBrowserView* flippass_db_browser_view_alloc(void) {
    FlipPassDbBrowserView* browser = malloc(sizeof(FlipPassDbBrowserView));
    browser->view = view_alloc();
    browser->scroll_timer =
        furi_timer_alloc(flippass_db_browser_view_timer_callback, FuriTimerTypePeriodic, browser);
    browser->callback = NULL;
    browser->callback_context = NULL;

    view_set_context(browser->view, browser);
    view_allocate_model(browser->view, ViewModelTypeLocking, sizeof(FlipPassDbBrowserViewModel));
    view_set_draw_callback(browser->view, flippass_db_browser_view_draw_callback);
    view_set_input_callback(browser->view, flippass_db_browser_view_input_callback);
    view_set_enter_callback(browser->view, flippass_db_browser_view_enter_callback);
    view_set_exit_callback(browser->view, flippass_db_browser_view_exit_callback);
    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        { model->draw_string = furi_string_alloc(); },
        false);

    flippass_db_browser_view_reset(browser);
    return browser;
}

void flippass_db_browser_view_free(FlipPassDbBrowserView* browser) {
    furi_check(browser);

    furi_timer_stop(browser->scroll_timer);
    furi_timer_free(browser->scroll_timer);
    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        {
            if(model->draw_string != NULL) {
                furi_string_free(model->draw_string);
                model->draw_string = NULL;
            }
        },
        false);
    view_free(browser->view);
    free(browser);
}

View* flippass_db_browser_view_get_view(FlipPassDbBrowserView* browser) {
    furi_check(browser);
    return browser->view;
}

void flippass_db_browser_view_set_callback(
    FlipPassDbBrowserView* browser,
    FlipPassDbBrowserViewCallback callback,
    void* context) {
    furi_check(browser);
    browser->callback = callback;
    browser->callback_context = context;
}

void flippass_db_browser_view_reset(FlipPassDbBrowserView* browser) {
    furi_check(browser);

    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        {
            model->header[0] = '\0';
            memset(model->items, 0, sizeof(model->items));
            model->item_count = 0U;
            model->selected_index = 0U;
            model->action_selected_index = 0U;
            model->scroll_counter = 0U;
            model->has_parent = false;
            model->action_menu_open = false;
            model->mode = FlipPassDbBrowserModeBrowse;
        },
        true);
}

void flippass_db_browser_view_set_header(FlipPassDbBrowserView* browser, const char* header) {
    furi_check(browser);

    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        { snprintf(model->header, sizeof(model->header), "%s", header != NULL ? header : ""); },
        true);
}

void flippass_db_browser_view_set_has_parent(FlipPassDbBrowserView* browser, bool has_parent) {
    furi_check(browser);
    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        { model->has_parent = has_parent; },
        true);
}

void flippass_db_browser_view_set_mode(
    FlipPassDbBrowserView* browser,
    FlipPassDbBrowserMode mode) {
    furi_check(browser);
    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        {
            model->mode = mode;
            model->action_menu_open = false;
            model->scroll_counter = 0U;
        },
        true);
}

void flippass_db_browser_view_add_item(
    FlipPassDbBrowserView* browser,
    FlipPassDbBrowserItemType type,
    const char* label) {
    furi_check(browser);

    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        {
            if(model->item_count < FLIPPASS_DB_BROWSER_MAX_ITEMS) {
                FlipPassDbBrowserItem* item = &model->items[model->item_count++];
                item->type = type;
                snprintf(item->label, sizeof(item->label), "%s", label != NULL ? label : "");
            }
        },
        true);
}

void flippass_db_browser_view_set_selected_item(FlipPassDbBrowserView* browser, uint32_t index) {
    furi_check(browser);

    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        {
            model->selected_index =
                (model->item_count == 0U) ? 0U : ((index < model->item_count) ? index : 0U);
            model->scroll_counter = 0U;
        },
        true);
}

uint32_t flippass_db_browser_view_get_selected_item(const FlipPassDbBrowserView* browser) {
    uint32_t selected = 0U;

    furi_check(browser);
    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        { selected = model->selected_index; },
        false);
    return selected;
}

void flippass_db_browser_view_set_action_selected(FlipPassDbBrowserView* browser, uint32_t index) {
    furi_check(browser);

    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        { model->action_selected_index = flippass_db_browser_clamp_action_index(index); },
        true);
}

uint32_t flippass_db_browser_view_get_action_selected(const FlipPassDbBrowserView* browser) {
    uint32_t selected = 0U;

    furi_check(browser);
    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        { selected = flippass_db_browser_clamp_action_index(model->action_selected_index); },
        false);
    return selected;
}

void flippass_db_browser_view_set_action_menu_open(FlipPassDbBrowserView* browser, bool open) {
    furi_check(browser);
    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        { model->action_menu_open = open; },
        true);
}

bool flippass_db_browser_view_is_action_menu_open(const FlipPassDbBrowserView* browser) {
    bool open = false;

    furi_check(browser);
    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        { open = model->action_menu_open; },
        false);
    return open;
}

FlipPassDbBrowserItemType
    flippass_db_browser_view_get_selected_type(const FlipPassDbBrowserView* browser) {
    FlipPassDbBrowserItemType type = FlipPassDbBrowserItemTypeInfo;

    furi_check(browser);
    with_view_model(
        browser->view,
        FlipPassDbBrowserViewModel * model,
        {
            if(model->item_count > 0U && model->selected_index < model->item_count) {
                type = model->items[model->selected_index].type;
            }
        },
        false);
    return type;
}
