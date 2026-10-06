#include "menu.h"

#include <gui/elements.h>
#include <assets_icons.h>
#include <furi.h>

#define TAG "Menu"

// Rate at which MenuModel::scroll_counter advances, i.e. how fast styles scroll a long label
#define MENU_SCROLL_INTERVAL_MS (333)

// Plugin ABI - see MENU_STYLE_PLUGIN_API_VERSION in menu.h. Every field here is pointer-width, so
// sizeof alone cannot see a reorder; these pin the offsets a style actually reads. Fix the assert
// AND bump the version, or already-built styles read the wrong fields.
static_assert(
    offsetof(MenuItem, label) == 0 && offsetof(MenuItem, icon) == 4 && sizeof(MenuItem) == 20,
    "MenuItem layout and stride are plugin ABI");
static_assert(
    offsetof(MenuModel, items) == 0 && offsetof(MenuModel, count) == 4 &&
        offsetof(MenuModel, position) == 8 && offsetof(MenuModel, scroll_counter) == 12 &&
        offsetof(MenuModel, offset) == 16 && sizeof(MenuModel) == 24,
    "MenuModel layout is plugin ABI - appending is compatible, moving a field is not");
static_assert(
    offsetof(MenuStylePlugin, draw) == 0 && sizeof(MenuStylePlugin) == 8,
    "MenuStylePlugin is the plugin-owned vtable");

struct Menu {
    View* view;
    FuriTimer* scroll_timer;
    size_t item_capacity; // Private: MenuModel/MenuItem remain compatible with style plugins.
    bool active; // Guarded by the view model mutex
};

static void menu_draw_callback(Canvas* canvas, void* _model) {
    MenuModel* model = _model;

    canvas_clear(canvas);

    if(model->position >= model->count) {
        canvas_draw_str(canvas, 2, 32, "Empty");
        elements_scrollbar(canvas, 0, 0);
    } else if(model->style) {
        model->style->draw(canvas, model);
    } else {
        for(size_t i = 0; i < 3; i++) {
            const MenuItem* item =
                &model->items[(model->position + model->count + i - 1) % model->count];
            canvas_set_font(canvas, i == 1 ? FontPrimary : FontSecondary);
            canvas_draw_icon_animation(
                canvas,
                4 + (14 - (int32_t)icon_animation_get_width(item->icon)) / 2,
                3 + 22 * i + (14 - (int32_t)icon_animation_get_height(item->icon)) / 2,
                item->icon);
            // Preserve RM label normalization and scrolling in the built-in fallback.
            FuriString* label = furi_string_alloc_set(item->label);
            if(furi_string_start_with_str(label, "[")) {
                size_t trim = furi_string_search_str(label, "] ", 1);
                if(trim != FURI_STRING_FAILURE) furi_string_right(label, trim + 2);
            }
            size_t scroll = (i == 1 && model->scroll_counter) ? model->scroll_counter - 1 : 0;
            elements_scrollable_text_line(canvas, 22, 14 + 22 * i, 98, label, scroll, false);
            furi_string_free(label);
        }
        elements_frame(canvas, 0, 21, 128 - 5, 21);
        elements_scrollbar(canvas, model->position, model->count);
    }
}

static void menu_set_position(Menu* menu, MenuModel* model, size_t position) {
    if(position >= model->count || position == model->position) return;
    // A style gets a mutable model, so the old position may have been written since it was checked
    if(menu->active && model->position < model->count) {
        icon_animation_stop(model->items[model->position].icon);
    }
    model->position = position;
    model->scroll_counter = 0;
    if(menu->active) icon_animation_start(model->items[position].icon);
}

static bool menu_process_move(Menu* menu, InputKey key) {
    bool consumed = false;
    bool dropped = false;
    size_t requested = 0;
    size_t count = 0;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            if(model->style) {
                consumed = true;
                if(model->position < model->count) {
                    requested = model->style->navigate(model, key);
                    count = model->count;
                    dropped = requested >= count;
                    menu_set_position(menu, model, requested);
                }
            } else if(key == InputKeyUp || key == InputKeyDown) {
                consumed = true;
                if(model->position < model->count) {
                    size_t position = model->position;
                    if(key == InputKeyUp) {
                        position = position ? position - 1 : model->count - 1;
                    } else {
                        position = (position + 1) % model->count;
                    }
                    menu_set_position(menu, model, position);
                }
            }
        },
        consumed);
    // Dropping this silently is what kept the C64 and Compact dead keys invisible
    if(dropped) {
        FURI_LOG_W(TAG, "Style asked for item %zu of %zu on key %d", requested, count, key);
    }
    return consumed;
}

static void menu_process_ok(Menu* menu) {
    MenuItemCallback callback = NULL;
    void* callback_context = NULL;
    uint32_t callback_index = 0;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            if(model->position < model->count) {
                const MenuItem* item = &model->items[model->position];
                callback = item->callback;
                callback_context = item->callback_context;
                callback_index = item->index;
            }
        },
        false);
    // The callback may reset the menu, so keep no pointer into its item array.
    if(callback) callback(callback_context, callback_index);
}

static bool menu_input_callback(InputEvent* event, void* context) {
    Menu* menu = context;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyOk:
        if(event->type != InputTypeShort) return false;
        menu_process_ok(menu);
        return true;
    case InputKeyUp:
    case InputKeyDown:
    case InputKeyLeft:
    case InputKeyRight:
        return menu_process_move(menu, event->key);
    default:
        return false;
    }
}

static void menu_scroll_timer_callback(void* context) {
    Menu* menu = context;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            if(menu->active) model->scroll_counter++;
        },
        true);
}

static void menu_enter(void* context) {
    Menu* menu = context;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            menu->active = true;
            if(model->position < model->count) {
                icon_animation_start(model->items[model->position].icon);
            }
            model->scroll_counter = 0;
        },
        false);
    furi_timer_start(menu->scroll_timer, furi_ms_to_ticks(MENU_SCROLL_INTERVAL_MS));
}

static void menu_exit(void* context) {
    Menu* menu = context;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            menu->active = false;
            if(model->position < model->count) {
                icon_animation_stop(model->items[model->position].icon);
            }
        },
        false);
    furi_timer_stop(menu->scroll_timer);
}

Menu* menu_alloc(void) {
    Menu* menu = malloc(sizeof(Menu));
    menu->active = false;
    menu->item_capacity = 0;
    menu->view = view_alloc();
    view_set_context(menu->view, menu);
    view_allocate_model(menu->view, ViewModelTypeLocking, sizeof(MenuModel));
    view_set_draw_callback(menu->view, menu_draw_callback);
    view_set_input_callback(menu->view, menu_input_callback);
    view_set_enter_callback(menu->view, menu_enter);
    view_set_exit_callback(menu->view, menu_exit);
    menu->scroll_timer = furi_timer_alloc(menu_scroll_timer_callback, FuriTimerTypePeriodic, menu);

    with_view_model(menu->view, MenuModel * model, { memset(model, 0, sizeof(MenuModel)); }, true);

    return menu;
}

void menu_free(Menu* menu) {
    furi_check(menu);

    // Defence in depth: with the timer already gone, menu_reset() below cannot meet a scroll
    // callback parked on the model mutex, whatever it does with the lock
    furi_timer_free(menu->scroll_timer);
    menu_reset(menu);
    view_free(menu->view);

    free(menu);
}

View* menu_get_view(Menu* menu) {
    furi_check(menu);
    return menu->view;
}

static void menu_reserve_item(Menu* menu, MenuModel* model) {
    if(model->count < menu->item_capacity) return;

    // Grow by 50% so large game lists do not copy the whole array for every FAP.
    // Keep this capacity private: menu style plugins still see exactly count items.
    const size_t capacity = menu->item_capacity ? menu->item_capacity + menu->item_capacity / 2 :
                                                  8;
    furi_check(capacity > menu->item_capacity && capacity <= SIZE_MAX / sizeof(MenuItem));
    MenuItem* items = malloc(capacity * sizeof(MenuItem));
    furi_check(items);
    // RM's realloc copies the requested new size; copy only the initialized old items.
    if(model->count) memcpy(items, model->items, model->count * sizeof(MenuItem));
    free(model->items);
    model->items = items;
    menu->item_capacity = capacity;
}

void menu_add_item(
    Menu* menu,
    const char* label,
    const Icon* icon,
    uint32_t index,
    MenuItemCallback callback,
    void* context) {
    furi_check(menu);
    furi_check(label);

    with_view_model(
        menu->view,
        MenuModel * model,
        {
            menu_reserve_item(menu, model);
            MenuItem* item = &model->items[model->count++];
            item->label = label;
            item->icon = icon_animation_alloc(icon ? icon : &A_Plugins_14);
            view_tie_icon_animation(menu->view, item->icon);
            item->index = index;
            item->callback = callback;
            item->callback_context = context;
            if(menu->active && model->count == 1) icon_animation_start(item->icon);
        },
        true);
}

void menu_reset(Menu* menu) {
    furi_check(menu);

    MenuItem* items = NULL;
    size_t count = 0;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            items = model->items;
            count = model->count;
            model->items = NULL;
            model->count = 0;
            menu->item_capacity = 0;
            model->position = 0;
            model->scroll_counter = 0;
            model->offset = 0;
        },
        true);

    // Only after the model has let go of them: icon_animation_free() blocks on the timer daemon,
    // which is where menu_scroll_timer_callback() waits for the model mutex
    for(size_t i = 0; i < count; i++) {
        icon_animation_free(items[i].icon);
    }
    free(items);
}

uint32_t menu_get_selected_item(Menu* menu) {
    furi_check(menu);
    uint32_t index = 0;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            if(model->position < model->count) index = model->items[model->position].index;
        },
        false);
    return index;
}

void menu_set_selected_item(Menu* menu, uint32_t index) {
    furi_check(menu);
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            size_t position = 0;
            while(position < model->count && model->items[position].index != index)
                position++;
            if(position >= model->count) position = 0;
            menu_set_position(menu, model, position);
            model->scroll_counter = 0;
            model->offset = 0;
        },
        true);
}

void menu_set_style(Menu* menu, const MenuStylePlugin* style) {
    furi_check(menu);
    if(style && (!style->draw || !style->navigate)) {
        FURI_LOG_W(TAG, "Incomplete menu style, using list");
        style = NULL;
    }

    with_view_model(
        menu->view,
        MenuModel * model,
        {
            model->style = style;
            model->scroll_counter = 0;
            model->offset = 0;
        },
        true);
}
