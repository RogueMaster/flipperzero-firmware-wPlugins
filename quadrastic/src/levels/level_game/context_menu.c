/*
 * Copyright 2025 Ivan Barsukov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "context_menu.h"

#include <stddef.h>

#include <m-array.h>

#include <furi.h>
#include <gui/elements.h>
#include <gui/modules/variable_item_list.h>

#include "src/engine/game_manager.h"
#include "src/game.h"
#include "src/gui_bridge/input_converter.h"
#include "src/levels/level_game/level_game.h"

#define ITEM_TEXT_PADDING_X 6
#define ITEM_TEXT_PADDING_Y 4
#define ITEM_SELECTION_PADDING 1
#define ITEMS_ON_SCREEN 2
// BOX_WIDTH - BOX_PADDING_X * 2 - SCROLLBAR_WIDTH - BOX_SPACING;
#define ITEM_WIDTH 70
#define ITEM_HEIGHT 16

#define SCROLLBAR_WIDTH 3

#define BOX_PADDING_X 4
#define BOX_PADDING_Y 3
#define BOX_SPACING 3

#define BOX_WIDTH 84
#define BOX_HEIGHT 38 // (ITEM_HEIGHT * ITEMS_ON_SCREEN + BOX_PADDING_Y * 2)
#define BOX_X 22      // (SCREEN_WIDTH - BOX_WIDTH) / 2)
#define BOX_Y 13      // (SCREEN_HEIGHT - BOX_HEIGHT) / 2)
#define BOX_RADIUS 4
#define BOX_OFFSET_X 26 // BOX_X + BOX_PADDING_X;
#define BOX_OFFSET_Y 16 // BOX_Y + BOX_PADDING_Y;

typedef struct
{
    FuriString* label;
    uint32_t index;
    ContextMenuItemCallback callback;
    void* callback_context;
} ContextMenuItem;

static void
ContextMenuItem_init(ContextMenuItem* item)
{
    item->label = furi_string_alloc();
    item->index = 0;
    item->callback = NULL;
    item->callback_context = NULL;
}

static void
ContextMenuItem_init_set(ContextMenuItem* item, const ContextMenuItem* src)
{
    item->label = furi_string_alloc_set(src->label);
    item->index = src->index;
    item->callback = src->callback;
    item->callback_context = src->callback_context;
}

static void
ContextMenuItem_set(ContextMenuItem* item, const ContextMenuItem* src)
{
    furi_string_set(item->label, src->label);
    item->index = src->index;
    item->callback = src->callback;
    item->callback_context = src->callback_context;
}

static void
ContextMenuItem_clear(ContextMenuItem* item)
{
    furi_string_free(item->label);
}

ARRAY_DEF(ContextMenuItemArray,
          ContextMenuItem,
          (INIT(API_2(ContextMenuItem_init)),
           SET(API_6(ContextMenuItem_set)),
           INIT_SET(API_6(ContextMenuItem_init_set)),
           CLEAR(API_2(ContextMenuItem_clear))))

typedef struct
{
    InputConverter* input_converter;

    ContextMenuItemArray_t items;
    size_t position;
    size_t window_position;

    ContextMenuBackCallback back_callback;
    void* back_callback_context;

} ContextMenuContext;

void
context_menu_add_item(Entity* self,
                      const char* label,
                      uint32_t index,
                      ContextMenuItemCallback callback,
                      void* callback_context)
{
    furi_check(self);
    furi_check(label);

    ContextMenuContext* entity_context = entity_context_get(self);

    ContextMenuItem* item;
    item = ContextMenuItemArray_push_new(entity_context->items);
    furi_string_set_str(item->label, label);
    item->index = index;
    item->callback = callback;
    item->callback_context = callback_context;
}

void
context_menu_back_callback_set(Entity* self,
                               ContextMenuBackCallback back_callback,
                               void* callback_context)
{
    furi_check(self);
    furi_check(back_callback);

    ContextMenuContext* entity_context = entity_context_get(self);
    entity_context->back_callback = back_callback;
    entity_context->back_callback_context = callback_context;
}

void
context_menu_reset_state(Entity* self)
{
    furi_check(self);

    ContextMenuContext* entity_context = entity_context_get(self);
    input_converter_reset(entity_context->input_converter);
    entity_context->position = 0;
    entity_context->window_position = 0;
}

static void
context_menu_start(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(self);
    UNUSED(manager);

    ContextMenuContext* entity_context = _entity_context;

    // Alloc converter
    entity_context->input_converter = input_converter_alloc();

    // Menu
    ContextMenuItemArray_init(entity_context->items);
    entity_context->position = 0;
    entity_context->window_position = 0;

    // Callback
    entity_context->back_callback = NULL;
    entity_context->back_callback_context = NULL;
}

static void
context_menu_stop(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(self);
    UNUSED(manager);

    ContextMenuContext* entity_context = _entity_context;
    ContextMenuItemArray_clear(entity_context->items);
    input_converter_free(entity_context->input_converter);
}

static void
context_menu_process_up(ContextMenuContext* context_menu)
{
    const size_t items_size = ContextMenuItemArray_size(context_menu->items);

    if (context_menu->position == 0) {
        context_menu->position = items_size - 1;
        if (context_menu->position > ITEMS_ON_SCREEN - 1) {
            context_menu->window_position =
              context_menu->position - (ITEMS_ON_SCREEN - 1);
        }
    } else {
        context_menu->position--;
        if (context_menu->position < context_menu->window_position) {
            context_menu->window_position--;
        }
    }
}

static void
context_menu_process_down(ContextMenuContext* context_menu)
{
    const size_t items_size = ContextMenuItemArray_size(context_menu->items);

    if (context_menu->position == items_size - 1) {
        context_menu->position = 0;
        context_menu->window_position = 0;
    } else {
        context_menu->position++;

        if ((context_menu->position - context_menu->window_position ==
             ITEMS_ON_SCREEN) &&
            (context_menu->window_position < items_size - ITEMS_ON_SCREEN)) {
            context_menu->window_position++;
        }
    }
}

static void
context_menu_process_ok(ContextMenuContext* context_menu)
{
    ContextMenuItem* item = NULL;

    const size_t items_size = ContextMenuItemArray_size(context_menu->items);
    if (context_menu->position < items_size) {
        item =
          ContextMenuItemArray_get(context_menu->items, context_menu->position);
    }

    if (item && item->callback) {
        item->callback(item->callback_context, item->index);
    }
}

static bool
context_menu_input_process(ContextMenuContext* context_menu, InputEvent* event)
{
    furi_assert(context_menu);
    furi_assert(event);

    bool consumed = false;

    if (event->type == InputTypeShort) {
        switch (event->key) {
            case InputKeyUp:
                consumed = true;
                context_menu_process_up(context_menu);
                break;
            case InputKeyDown:
                consumed = true;
                context_menu_process_down(context_menu);
                break;
            case InputKeyOk:
                consumed = true;
                context_menu_process_ok(context_menu);
                break;
            default:
                break;
        }
    } else if (event->type == InputTypeRepeat) {
        if (event->key == InputKeyUp) {
            consumed = true;
            context_menu_process_up(context_menu);
        } else if (event->key == InputKeyDown) {
            consumed = true;
            context_menu_process_down(context_menu);
        }
    }

    return consumed;
}

static bool
need_call_back_callback(ContextMenuContext* entity_context, InputEvent* event)
{
    return event->key == InputKeyBack && event->type == InputTypeShort &&
           entity_context->back_callback != NULL;
}

static void
context_menu_update(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(self);

    // Check pause
    Level* level = game_manager_current_level_get(manager);
    GameLevelContext* level_context = level_context_get(level);
    if (!level_context->is_paused) {
        return;
    }

    ContextMenuContext* entity_context = _entity_context;

    // Get input state and convert to input events
    InputState input = game_manager_input_get(manager);
    input_converter_process_state(entity_context->input_converter, &input);

    // Process events
    InputEvent event;
    while (input_converter_get_event(entity_context->input_converter, &event) ==
           FuriStatusOk) {
        if (need_call_back_callback(entity_context, &event)) {
            entity_context->back_callback(
              entity_context->back_callback_context);
            return;
        }
        context_menu_input_process(entity_context, &event);
    }
}

static void
gray_canvas(Canvas* const canvas)
{
    canvas_set_color(canvas, ColorWhite);
    for (size_t x = 0; x < SCREEN_WIDTH; x += 2) {
        for (size_t y = 0; y < SCREEN_HEIGHT; y++) {
            canvas_draw_dot(canvas, x + (y % 2 == 1 ? 0 : 1), y);
        }
    }
    canvas_set_color(canvas, ColorBlack);
}

static void
draw_box(Canvas* const canvas)
{
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, BOX_X, BOX_Y, BOX_WIDTH, BOX_HEIGHT, BOX_RADIUS);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, BOX_X, BOX_Y, BOX_WIDTH, BOX_HEIGHT, BOX_RADIUS);
}

static void
draw_items(Canvas* const canvas, ContextMenuContext* menu_context)
{
    canvas_set_font(canvas, FontSecondary);

    size_t index = 0;
    ContextMenuItemArray_it_t it;
    for (ContextMenuItemArray_it(it, menu_context->items);
         !ContextMenuItemArray_end_p(it);
         ContextMenuItemArray_next(it)) {

        const size_t item_position = index - menu_context->window_position;

        if (item_position < ITEMS_ON_SCREEN) {
            if (index == menu_context->position) {
                canvas_set_color(canvas, ColorBlack);
                elements_slightly_rounded_box(
                  canvas,
                  BOX_OFFSET_X,
                  BOX_OFFSET_Y + (item_position * ITEM_HEIGHT) + 1,
                  ITEM_WIDTH,
                  ITEM_HEIGHT - 2);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }

            FuriString* display_str;
            display_str =
              furi_string_alloc_set(ContextMenuItemArray_cref(it)->label);

            elements_string_fit_width(
              canvas, display_str, ITEM_WIDTH - (ITEM_TEXT_PADDING_X * 2));

            canvas_draw_str(canvas,
                            BOX_OFFSET_X + ITEM_TEXT_PADDING_X,
                            BOX_OFFSET_Y + (item_position * ITEM_HEIGHT) +
                              ITEM_HEIGHT - ITEM_TEXT_PADDING_Y,
                            furi_string_get_cstr(display_str));

            furi_string_free(display_str);
        }

        ++index;
    }
}

static void
draw_scrollbar(Canvas* const canvas, ContextMenuContext* menu_context)
{
    elements_scrollbar_pos(canvas,
                           BOX_X + BOX_WIDTH - BOX_PADDING_X,
                           BOX_Y + BOX_PADDING_Y + ITEM_SELECTION_PADDING,
                           BOX_HEIGHT - BOX_PADDING_Y * 2 -
                             ITEM_SELECTION_PADDING * 2,
                           menu_context->position,
                           ContextMenuItemArray_size(menu_context->items));
}

static void
context_menu_render(Entity* self,
                    GameManager* manager,
                    Canvas* canvas,
                    void* _entity_context)
{
    UNUSED(self);

    Level* level = game_manager_current_level_get(manager);
    GameLevelContext* level_context = level_context_get(level);
    if (!level_context->is_paused) {
        return;
    }

    ContextMenuContext* menu_context = _entity_context;

    gray_canvas(canvas);
    draw_box(canvas);
    draw_items(canvas, menu_context);
    draw_scrollbar(canvas, menu_context);
}

const EntityDescription context_menu_description = {
    .start = context_menu_start,
    .stop = context_menu_stop,
    .update = context_menu_update,
    .render = context_menu_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(ContextMenuContext),
};
