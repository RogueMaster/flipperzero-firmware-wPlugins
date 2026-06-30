#include "mainmenu.h"

#include <gui/elements.h>
#include <furi.h>
#include <m-array.h>

#include <ghost_esp_icons.h>

#define CARD_WIDTH         28
#define CARD_HEIGHT        44
#define CARD_MARGIN        1
#define BASE_Y_POSITION    5
#define SELECTED_OFFSET    3
#define ICON_WIDTH         20
#define ICON_PADDING       3
#define TEXT_BOTTOM_MARGIN 8
#define BUTTON_HEIGHT      12
#define MAX_VISIBLE_CARDS  4
#define SCROLL_ARROW_SIZE  4

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
} CardLayout;

struct MainMenu {
    View* view;
    FuriTimer* locked_timer;
    MainMenuItemCallback help_callback;
    void* help_context;
};

typedef struct {
    FuriString* label;
    uint32_t index;
    MainMenuItemCallback callback;
    void* callback_context;
    bool locked;
    FuriString* locked_message;
} MainMenuItem;

static void MainMenuItem_init(MainMenuItem* item) {
    item->label = furi_string_alloc();
    item->index = 0;
    item->callback = NULL;
    item->callback_context = NULL;
    item->locked = false;
    item->locked_message = furi_string_alloc();
}

static void MainMenuItem_init_set(MainMenuItem* item, const MainMenuItem* src) {
    item->label = furi_string_alloc_set(src->label);
    item->index = src->index;
    item->callback = src->callback;
    item->callback_context = src->callback_context;
    item->locked = src->locked;
    item->locked_message = furi_string_alloc_set(src->locked_message);
}

static void MainMenuItem_set(MainMenuItem* item, const MainMenuItem* src) {
    furi_string_set(item->label, src->label);
    item->index = src->index;
    item->callback = src->callback;
    item->callback_context = src->callback_context;
    item->locked = src->locked;
    furi_string_set(item->locked_message, src->locked_message);
}

static void MainMenuItem_clear(MainMenuItem* item) {
    furi_string_free(item->label);
    furi_string_free(item->locked_message);
}

ARRAY_DEF(
    MainMenuItemArray,
    MainMenuItem,
    (INIT(API_2(MainMenuItem_init)),
     SET(API_6(MainMenuItem_set)),
     INIT_SET(API_6(MainMenuItem_init_set)),
     CLEAR(API_2(MainMenuItem_clear))))

typedef struct {
    MainMenuItemArray_t items;
    FuriString* header;
    size_t position;
    size_t window_position;
    float scroll_offset; // 0.0 to 1.0 for smooth animation
    bool locked_message_visible;
    bool is_vertical;
} MainMenuModel;

static void main_menu_process_up(MainMenu* main_menu);
static void main_menu_process_down(MainMenu* main_menu);
static void main_menu_process_ok(MainMenu* main_menu);

static size_t main_menu_items_on_screen(MainMenuModel* model) {
    UNUSED(model);
    return MAX_VISIBLE_CARDS;
}

static bool is_valid_icon_index(size_t position) {
    return position <= 4;
}

// Add the new helper function:
void main_menu_set_help_callback(MainMenu* main_menu, MainMenuItemCallback callback, void* context) {
    furi_assert(main_menu);
    main_menu->help_callback = callback;
    main_menu->help_context = context;
}

static void draw_help_button(Canvas* canvas) {
    const char* str = "Help";
    const size_t vertical_offset = 3;
    const size_t horizontal_offset = 3;
    const size_t string_width = canvas_string_width(canvas, str);

    // Create a small arrow icon directly
    const uint8_t arrow_width = 7;
    const uint8_t arrow_height = 4;
    const int32_t icon_h_offset = 3;
    const int32_t icon_width_with_offset = arrow_width + icon_h_offset;
    const size_t button_width = string_width + horizontal_offset * 2 + icon_width_with_offset;

    const int32_t x = (canvas_width(canvas) - button_width) / 2;
    const int32_t y = canvas_height(canvas);

    // Draw text
    canvas_draw_str(canvas, x + horizontal_offset, y - vertical_offset, str);

    // Draw small down arrow
    const int32_t arrow_x = x + horizontal_offset + string_width + icon_h_offset;
    const int32_t arrow_y = y - vertical_offset - arrow_height;
    canvas_draw_line(canvas, arrow_x + 3, arrow_y + 3, arrow_x + 6, arrow_y);
    canvas_draw_line(canvas, arrow_x + 3, arrow_y + 3, arrow_x, arrow_y);
}

static CardLayout calculate_card_layout(
    Canvas* canvas,
    size_t total_cards,
    size_t visible_cards,
    size_t position,
    size_t window_position,
    float scroll_offset,
    bool is_selected) {
    UNUSED(total_cards);
    CardLayout layout = {0};

    if(visible_cards == 0) {
        return layout;
    }

    // Calculate relative position within the visible window
    int32_t relative_pos = (int32_t)position - (int32_t)window_position;

    // Apply scroll offset for smooth animation
    const uint8_t actual_visible = visible_cards < MAX_VISIBLE_CARDS ? visible_cards :
                                                                       MAX_VISIBLE_CARDS;
    const uint8_t total_width = actual_visible * CARD_WIDTH + (actual_visible - 1) * CARD_MARGIN;
    const int32_t scroll_pixels = (int32_t)(scroll_offset * (CARD_WIDTH + CARD_MARGIN));

    layout.x = (canvas_width(canvas) - total_width) / 2 +
               relative_pos * (CARD_WIDTH + CARD_MARGIN) - scroll_pixels;

    layout.y = is_selected ? BASE_Y_POSITION - SELECTED_OFFSET : BASE_Y_POSITION;
    layout.width = CARD_WIDTH;
    layout.height = CARD_HEIGHT;

    return layout;
}

static void draw_card_background(Canvas* canvas, const CardLayout* layout, bool is_selected) {
    // Draw shadow effect
    canvas_set_color(canvas, ColorBlack);
    elements_slightly_rounded_box(
        canvas, layout->x + 2, layout->y + 2, layout->width, layout->height);

    // Draw main background
    canvas_set_color(canvas, is_selected ? ColorBlack : ColorWhite);
    elements_slightly_rounded_box(canvas, layout->x, layout->y, layout->width, layout->height);

    // Draw outline
    canvas_set_color(canvas, ColorBlack);
    elements_slightly_rounded_frame(canvas, layout->x, layout->y, layout->width, layout->height);
}

static void
    draw_card_icon(Canvas* canvas, const CardLayout* layout, const Icon* icon, bool is_selected) {
    const uint8_t icon_x = layout->x + (layout->width - ICON_WIDTH) / 2;
    const uint8_t icon_y = layout->y + ICON_PADDING;
    // Draw main icon
    canvas_set_color(canvas, is_selected ? ColorWhite : ColorBlack);
    canvas_draw_icon(canvas, icon_x, icon_y, icon);
}

static void draw_card_label(
    Canvas* canvas,
    const CardLayout* layout,
    FuriString* label,
    bool is_selected,
    bool is_last_card) {
    elements_string_fit_width(canvas, label, layout->width + 40);

    // Draw text shadow first
    if(!is_selected) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(
            canvas,
            is_last_card ? layout->x + 4 : layout->x + 6, // Offset by 1 for shadow
            layout->y + layout->height - TEXT_BOTTOM_MARGIN + 1,
            furi_string_get_cstr(label));
    }

    // Draw main text
    canvas_set_color(canvas, is_selected ? ColorWhite : ColorBlack);
    canvas_draw_str(
        canvas,
        is_last_card ? layout->x + 3 : layout->x + 5,
        layout->y + layout->height - TEXT_BOTTOM_MARGIN,
        furi_string_get_cstr(label));
}
static void main_menu_view_draw_callback(Canvas* canvas, void* _model) {
    MainMenuModel* model = _model;
    const size_t total_cards = MainMenuItemArray_size(model->items);
    const size_t visible_cards = total_cards < MAX_VISIBLE_CARDS ? total_cards : MAX_VISIBLE_CARDS;

    canvas_clear(canvas);

    // Draw only visible menu items (within the window)
    size_t position = 0;
    MainMenuItemArray_it_t it;
    for(MainMenuItemArray_it(it, model->items); !MainMenuItemArray_end_p(it);
        MainMenuItemArray_next(it)) {
        // Only draw cards within visible window (with one card buffer for smooth animation)
        if(position >= model->window_position &&
           position < model->window_position + MAX_VISIBLE_CARDS + 1) {
            const bool is_selected = (position == model->position);
            const bool is_last_card = position + 1 == total_cards;

            CardLayout layout = calculate_card_layout(
                canvas,
                total_cards,
                visible_cards,
                position,
                model->window_position,
                model->scroll_offset,
                is_selected);

            draw_card_background(canvas, &layout, is_selected);

            const Icon* icon = NULL;
            if(is_valid_icon_index(position)) {
                switch(position) {
                case 0:
                    icon = &I_Wifi_icon;
                    break;
                case 1:
                    icon = &I_BLE_icon;
                    break;
                case 2:
                    icon = &I_GPS;
                    break; // Use GPS icon for position 2
                case 3:
                    icon = &I_infrared;
                    break; // Use Infrared icon for position 3
                case 4:
                    icon = &I_Cog;
                    break; // Use Settings icon for position 4
                }
            }

            if(icon) {
                draw_card_icon(canvas, &layout, icon, is_selected);
            }

            FuriString* disp_str = furi_string_alloc_set(MainMenuItemArray_cref(it)->label);
            draw_card_label(canvas, &layout, disp_str, is_selected, is_last_card);
            furi_string_free(disp_str);
        }

        position++;
    }

    // Draw help button last
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);
    draw_help_button(canvas);
    canvas_set_color(canvas, ColorBlack);
}
static bool main_menu_view_input_callback(InputEvent* event, void* context) {
    MainMenu* main_menu = context;
    furi_assert(main_menu);
    bool consumed = false;

    bool locked_message_visible = false;
    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        { locked_message_visible = model->locked_message_visible; },
        false);

    if((event->type != InputTypePress && event->type != InputTypeRelease) &&
       locked_message_visible) {
        with_view_model(
            main_menu->view,
            MainMenuModel * model,
            { model->locked_message_visible = false; },
            true);
        consumed = true;
    } else if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyLeft:
            consumed = true;
            main_menu_process_up(main_menu);
            break;
        case InputKeyRight:
            consumed = true;
            main_menu_process_down(main_menu);
            break;
        case InputKeyOk:
            consumed = true;
            main_menu_process_ok(main_menu);
            break;
        case InputKeyDown:
            // Handle help button press
            if(main_menu->help_callback) {
                main_menu->help_callback(main_menu->help_context, 0);
                consumed = true;
            }
            break;
        default:
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            consumed = true;
            main_menu_process_up(main_menu);
        } else if(event->key == InputKeyDown) {
            consumed = true;
            main_menu_process_down(main_menu);
        }
    }

    return consumed;
}

void main_menu_timer_callback(void* context) {
    furi_assert(context);
    MainMenu* main_menu = context;

    with_view_model(
        main_menu->view, MainMenuModel * model, { model->locked_message_visible = false; }, true);
}

MainMenu* main_menu_alloc(void) {
    MainMenu* main_menu = malloc(sizeof(MainMenu));
    main_menu->view = view_alloc();
    view_set_context(main_menu->view, main_menu);
    view_allocate_model(main_menu->view, ViewModelTypeLocking, sizeof(MainMenuModel));
    view_set_draw_callback(main_menu->view, main_menu_view_draw_callback);
    view_set_input_callback(main_menu->view, main_menu_view_input_callback);

    main_menu->locked_timer =
        furi_timer_alloc(main_menu_timer_callback, FuriTimerTypeOnce, main_menu);

    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            MainMenuItemArray_init(model->items);
            model->position = 0;
            model->window_position = 0;
            model->scroll_offset = 0.0f;
            model->header = furi_string_alloc();
        },
        true);

    return main_menu;
}

void main_menu_free(MainMenu* main_menu) {
    furi_check(main_menu);

    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            furi_string_free(model->header);
            MainMenuItemArray_clear(model->items);
        },
        true);
    furi_timer_stop(main_menu->locked_timer);
    furi_timer_free(main_menu->locked_timer);
    view_free(main_menu->view);
    free(main_menu);
}

View* main_menu_get_view(MainMenu* main_menu) {
    furi_check(main_menu);
    return main_menu->view;
}

void main_menu_add_item(
    MainMenu* main_menu,
    const char* label,
    uint32_t index,
    MainMenuItemCallback callback,
    void* callback_context) {
    main_menu_add_lockable_item(main_menu, label, index, callback, callback_context, false, NULL);
}

void main_menu_add_lockable_item(
    MainMenu* main_menu,
    const char* label,
    uint32_t index,
    MainMenuItemCallback callback,
    void* callback_context,
    bool locked,
    const char* locked_message) {
    MainMenuItem* item = NULL;
    furi_check(label);
    furi_check(main_menu);
    if(locked) {
        furi_check(locked_message);
    }

    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            item = MainMenuItemArray_push_new(model->items);
            furi_string_set_str(item->label, label);
            item->index = index;
            item->callback = callback;
            item->callback_context = callback_context;
            item->locked = locked;
            if(locked) {
                furi_string_set_str(item->locked_message, locked_message);
            }
        },
        true);
}

void main_menu_change_item_label(MainMenu* main_menu, uint32_t index, const char* label) {
    furi_check(main_menu);
    furi_check(label);

    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            MainMenuItemArray_it_t it;
            for(MainMenuItemArray_it(it, model->items); !MainMenuItemArray_end_p(it);
                MainMenuItemArray_next(it)) {
                if(index == MainMenuItemArray_cref(it)->index) {
                    furi_string_set_str(MainMenuItemArray_cref(it)->label, label);
                    break;
                }
            }
        },
        true);
}

void main_menu_reset(MainMenu* main_menu) {
    furi_check(main_menu);
    view_set_orientation(main_menu->view, ViewOrientationHorizontal);

    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            MainMenuItemArray_reset(model->items);
            model->position = 0;
            model->window_position = 0;
            model->scroll_offset = 0.0f;
            model->is_vertical = false;
            furi_string_reset(model->header);
        },
        true);
}

uint32_t main_menu_get_selected_item(MainMenu* main_menu) {
    furi_check(main_menu);

    uint32_t selected_item_index = 0;

    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            if(model->position < MainMenuItemArray_size(model->items)) {
                const MainMenuItem* item = MainMenuItemArray_cget(model->items, model->position);
                selected_item_index = item->index;
            }
        },
        false);

    return selected_item_index;
}

void main_menu_set_selected_item(MainMenu* main_menu, uint32_t index) {
    furi_check(main_menu);
    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            size_t position = 0;
            MainMenuItemArray_it_t it;
            for(MainMenuItemArray_it(it, model->items); !MainMenuItemArray_end_p(it);
                MainMenuItemArray_next(it)) {
                if(index == MainMenuItemArray_cref(it)->index) {
                    break;
                }
                position++;
            }

            const size_t items_size = MainMenuItemArray_size(model->items);

            if(position >= items_size) {
                position = 0;
            }

            model->position = position;
            model->window_position = position;

            if(model->window_position > 0) {
                model->window_position -= 1;
            }

            const size_t items_on_screen = main_menu_items_on_screen(model);

            if(items_size <= items_on_screen) {
                model->window_position = 0;
            } else {
                const size_t pos = items_size - items_on_screen;
                if(model->window_position > pos) {
                    model->window_position = pos;
                }
            }
        },
        true);
}

void main_menu_process_up(MainMenu* main_menu) {
    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            const size_t items_on_screen = main_menu_items_on_screen(model);
            const size_t items_size = MainMenuItemArray_size(model->items);

            if(model->position > 0) {
                model->position--;
                // Adjust window if we're moving left and at the left edge of visible area
                if(model->position < model->window_position) {
                    model->window_position = model->position;
                }
                model->scroll_offset = 0.0f; // Reset for instant movement (can be animated later)
            } else {
                // Wrap to end
                model->position = items_size - 1;
                if(items_size > items_on_screen) {
                    model->window_position = items_size - items_on_screen;
                } else {
                    model->window_position = 0;
                }
                model->scroll_offset = 0.0f;
            }
        },
        true);
}

void main_menu_process_down(MainMenu* main_menu) {
    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            const size_t items_on_screen = main_menu_items_on_screen(model);
            const size_t items_size = MainMenuItemArray_size(model->items);

            if(model->position < items_size - 1) {
                model->position++;
                // Adjust window if we're moving right and at the right edge of visible area
                if(model->position >= model->window_position + items_on_screen) {
                    model->window_position = model->position - items_on_screen + 1;
                }
                model->scroll_offset = 0.0f; // Reset for instant movement (can be animated later)
            } else {
                // Wrap to beginning
                model->position = 0;
                model->window_position = 0;
                model->scroll_offset = 0.0f;
            }
        },
        true);
}

void main_menu_process_ok(MainMenu* main_menu) {
    MainMenuItem* item = NULL;

    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            const size_t items_size = MainMenuItemArray_size(model->items);
            if(model->position < items_size) {
                item = MainMenuItemArray_get(model->items, model->position);
            }
            if(item && item->locked) {
                model->locked_message_visible = true;
                furi_timer_start(main_menu->locked_timer, furi_kernel_get_tick_frequency() * 3);
            }
        },
        true);

    if(item && !item->locked && item->callback) {
        item->callback(item->callback_context, item->index);
    }
}

void main_menu_set_header(MainMenu* main_menu, const char* header) {
    furi_check(main_menu);

    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            if(header == NULL) {
                furi_string_reset(model->header);
            } else {
                furi_string_set_str(model->header, header);
            }
        },
        true);
}

void main_menu_set_orientation(MainMenu* main_menu, ViewOrientation orientation) {
    furi_check(main_menu);
    const bool is_vertical = orientation == ViewOrientationVertical ||
                             orientation == ViewOrientationVerticalFlip;

    view_set_orientation(main_menu->view, orientation);

    with_view_model(
        main_menu->view,
        MainMenuModel * model,
        {
            model->is_vertical = is_vertical;

            // Recalculating the position
            // Need if _set_orientation is called after _set_selected_item
            size_t position = model->position;
            const size_t items_size = MainMenuItemArray_size(model->items);
            const size_t items_on_screen = main_menu_items_on_screen(model);

            if(position >= items_size) {
                position = 0;
            }

            model->position = position;
            model->window_position = position;

            if(model->window_position > 0) {
                model->window_position -= 1;
            }

            if(items_size <= items_on_screen) {
                model->window_position = 0;
            } else {
                const size_t pos = items_size - items_on_screen;
                if(model->window_position > pos) {
                    model->window_position = pos;
                }
            }
        },
        true);
}
