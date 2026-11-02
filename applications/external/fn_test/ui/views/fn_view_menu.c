#include "fn_view_menu.h"

#include "../fn_draw.h"
#include "../fn_strings.h"
#include "../fn_ui.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>

#include <furi.h>

#define FN_MENU_ITEMS    5
#define FN_MENU_ITEM_W   (128 - 2 * FN_UI_MARGIN_X)
#define FN_MENU_MAX_ROWS 2

typedef struct {
    uint8_t selected;
} FnMenuModel;

struct FnMenuView {
    View* view;
    FnMenuCallback callback;
    void* callback_context;
};

static const char* const fn_menu_items[FN_MENU_ITEMS] = {
    FN_STR_MENU_FN_INFO,
    FN_STR_MENU_LIFE_INFO,
    FN_STR_MENU_FLASH_MGM,
    FN_STR_MENU_WIRING,
    FN_STR_MENU_ABOUT,
};

static uint8_t fn_view_menu_item_rows(Canvas* canvas, uint8_t index) {
    fn_draw_small(canvas);
    const uint16_t height = fn_draw_measure(
        canvas, fn_menu_items[index], FN_MENU_ITEM_W, FN_UI_SMALL_H, FN_MENU_MAX_ROWS);
    const uint8_t rows = (uint8_t)((height + FN_UI_SMALL_H - 1) / FN_UI_SMALL_H);
    return rows == 0 ? 1 : rows;
}

static void fn_view_menu_draw_callback(Canvas* canvas, void* context) {
    FnMenuModel* model = context;
    furi_assert(model);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_box(canvas, 0, 0, 128, FN_UI_HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    fn_draw_primary(canvas);
    canvas_draw_str(canvas, FN_UI_MARGIN_X, FN_UI_HEADER_H - 3, FN_STR_APP_TITLE);
    canvas_set_color(canvas, ColorBlack);

    int32_t y = FN_UI_CONTENT_Y;
    for(uint8_t i = 0; i < FN_MENU_ITEMS; ++i) {
        const uint8_t rows = fn_view_menu_item_rows(canvas, i);
        const int32_t item_h = rows * FN_UI_SMALL_H;
        const bool selected = (i == model->selected);

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
            /* +2: подсветка закрывает верхний выносной элемент и нижние
             * хвосты «ф», «ц», «щ» — иначе они теряются на белом. */
            canvas_draw_box(canvas, 0, y - 1, 128, item_h + 2);
            canvas_set_color(canvas, ColorWhite);
        }

        fn_draw_small(canvas);
        fn_draw_wrapped(
            canvas,
            FN_UI_MARGIN_X,
            y,
            fn_menu_items[i],
            FN_MENU_ITEM_W,
            FN_UI_SMALL_H,
            FN_MENU_MAX_ROWS);

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
        }
        y += item_h + 1;
    }
}

static bool fn_view_menu_input_callback(InputEvent* event, void* context) {
    FnMenuView* menu = context;
    furi_assert(menu);

    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp || event->key == InputKeyDown) {
            with_view_model(
                menu->view,
                FnMenuModel * model,
                {
                    if(event->key == InputKeyUp) {
                        model->selected = (model->selected == 0) ? (FN_MENU_ITEMS - 1) :
                                                                   (model->selected - 1);
                    } else {
                        model->selected = (model->selected + 1) % FN_MENU_ITEMS;
                    }
                },
                true);
            consumed = true;
        } else if(event->key == InputKeyOk) {
            uint8_t selected = 0;
            with_view_model(
                menu->view, FnMenuModel * model, { selected = model->selected; }, false);
            if(menu->callback) {
                menu->callback(menu->callback_context, selected);
            }
            consumed = true;
        }
    }

    return consumed;
}

FnMenuView* fn_view_menu_alloc(void) {
    FnMenuView* menu = malloc(sizeof(FnMenuView));
    menu->view = view_alloc();
    view_set_context(menu->view, menu);
    view_allocate_model(menu->view, ViewModelTypeLocking, sizeof(FnMenuModel));
    view_set_draw_callback(menu->view, fn_view_menu_draw_callback);
    view_set_input_callback(menu->view, fn_view_menu_input_callback);

    with_view_model(menu->view, FnMenuModel * model, { model->selected = 0; }, false);
    return menu;
}

void fn_view_menu_free(FnMenuView* menu) {
    if(!menu) return;
    view_free(menu->view);
    free(menu);
}

View* fn_view_menu_get_view(FnMenuView* menu) {
    return menu ? menu->view : NULL;
}

void fn_view_menu_set_callback(FnMenuView* menu, FnMenuCallback callback, void* context) {
    if(!menu) return;
    menu->callback = callback;
    menu->callback_context = context;
}

void fn_view_menu_set_selected(FnMenuView* menu, uint8_t index) {
    if(!menu || index >= FN_MENU_ITEMS) return;
    with_view_model(menu->view, FnMenuModel * model, { model->selected = index; }, true);
}

uint8_t fn_view_menu_get_selected(const FnMenuView* menu) {
    if(!menu) return 0;
    uint8_t selected = 0;
    with_view_model(menu->view, FnMenuModel * model, { selected = model->selected; }, false);
    return selected;
}
