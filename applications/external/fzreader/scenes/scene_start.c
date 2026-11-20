/**
 * @file scene_start.c
 * @brief Start menu and controls reference.
 *
 * The app has no scene manager: it keeps the screen in the model and dispatches
 * drawing and input from fzreader_app.c. These two screens are plain canvas
 * output and key handling, they own no state besides the highlighted item that
 * lives in the model.
 *
 * The menu is the entry point when no book could be resumed: it offers the file
 * browser and the controls reference. Back leaves the app.
 */

#include "scene_start.h"

#include <furi.h>
#include <gui/canvas.h>

#include "../fonts/reader_fonts.h"

#define SCENE_START_MARGIN     1
/** Fonts of the title, of the menu items and of the hints. */
#define SCENE_START_TITLE_FONT 1
#define SCENE_START_ITEM_FONT  2
#define SCENE_START_HINT_FONT  0
/** Distance between the baselines of two menu items. */
#define SCENE_START_ITEM_STEP  16
/** Height of the inverted band drawn behind the selected item. */
#define SCENE_START_BAND       15
/** Gap between the key column of the controls reference and its actions. */
#define SCENE_HELP_ACTION_GAP  4
/** Baseline of the hint line at the bottom of the controls reference. */
#define SCENE_HELP_HINT_Y      62

_Static_assert(READER_FONT_COUNT > SCENE_START_ITEM_FONT, "menu font index out of range");

static const char* const scene_start_items[FZREADER_START_ITEMS] = {
    "Open a book",
    "Help",
};

_Static_assert(
    COUNT_OF(scene_start_items) == FZREADER_START_ITEMS,
    "start menu item count mismatch");

/* Keys and their actions, one row per line of the reference. */
static const char* const scene_help_keys[] = {
    "Left / Right",
    "hold Up / Down",
    "Ok",
    "Back",
    "hold Back",
};

static const char* const scene_help_actions[] = {
    "page",
    "font",
    "info / jump",
    "open a book",
    "save and exit",
};

_Static_assert(
    COUNT_OF(scene_help_keys) == COUNT_OF(scene_help_actions),
    "controls reference rows mismatch");

void scene_start_draw(Canvas* canvas, FzReaderApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    const int32_t center = (int32_t)(canvas_width(canvas) / 2);

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_START_TITLE_FONT));
    canvas_draw_str_aligned(canvas, center, 7, AlignCenter, AlignCenter, "Reader");

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_START_ITEM_FONT));
    for(size_t index = 0; index < FZREADER_START_ITEMS; index++) {
        const char* item = scene_start_items[index];
        const int32_t y = 24 + (int32_t)index * SCENE_START_ITEM_STEP;
        const bool selected = index == app->start_selection;
        const int32_t width = (int32_t)canvas_string_width(canvas, item);

        if(selected) {
            /* The selection is the only inversion on the screen: no marker needed. */
            canvas_draw_box(
                canvas,
                center - width / 2 - 3,
                y - SCENE_START_BAND / 2,
                width + 6,
                SCENE_START_BAND);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, center, y, AlignCenter, AlignCenter, item);
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_START_HINT_FONT));
    canvas_draw_str_aligned(canvas, center, 59, AlignCenter, AlignCenter, "OK select, Back exit");
}

bool scene_start_input(FzReaderApp* app, const InputEvent* event) {
    bool redraw = false;

    switch(event->key) {
    case InputKeyUp:
        if(event->type == InputTypePress) {
            app->start_selection =
                (app->start_selection + FZREADER_START_ITEMS - 1) % FZREADER_START_ITEMS;
            redraw = true;
        }
        break;
    case InputKeyDown:
        if(event->type == InputTypePress) {
            app->start_selection = (app->start_selection + 1) % FZREADER_START_ITEMS;
            redraw = true;
        }
        break;
    case InputKeyOk:
        if(event->type == InputTypePress) {
            if(app->start_selection == 0) {
                /* A cancelled dialog leaves the menu where it was. */
                fzreader_app_pick_book(app);
            } else {
                app->screen = FzReaderScreenHelp;
            }
            redraw = true;
        }
        break;
    case InputKeyBack:
        if(event->type == InputTypePress) {
            app->running = false;
        }
        break;
    default:
        break;
    }

    return redraw;
}

void scene_help_draw(Canvas* canvas, FzReaderApp* app) {
    UNUSED(app);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_START_TITLE_FONT));
    canvas_draw_str_aligned(
        canvas, (int32_t)(canvas_width(canvas) / 2), 6, AlignCenter, AlignCenter, "Controls");

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_START_HINT_FONT));

    /* The actions follow the key column, whatever the width its longest key has. */
    int32_t action_x = SCENE_START_MARGIN;
    for(size_t index = 0; index < COUNT_OF(scene_help_keys); index++) {
        const int32_t width =
            SCENE_START_MARGIN + (int32_t)canvas_string_width(canvas, scene_help_keys[index]);
        if(width > action_x) {
            action_x = width;
        }
    }
    action_x += SCENE_HELP_ACTION_GAP;

    for(size_t index = 0; index < COUNT_OF(scene_help_keys); index++) {
        /* The reference is readonly, so the baselines can be counted from the font height. */
        const int32_t baseline = 16 + (int32_t)index * 8;
        canvas_draw_str(canvas, SCENE_START_MARGIN, baseline, scene_help_keys[index]);
        canvas_draw_str(canvas, action_x, baseline, scene_help_actions[index]);
    }

    /* Back belongs to the reference too: it leaves for the menu or for the app. */
    canvas_draw_str_aligned(
        canvas,
        (int32_t)(canvas_width(canvas) / 2),
        SCENE_HELP_HINT_Y,
        AlignCenter,
        AlignBottom,
        "Back: menu, hold Back: exit");
}

bool scene_help_input(FzReaderApp* app, const InputEvent* event) {
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort) {
            app->screen = FzReaderScreenStart;
            return true;
        }
        if(event->type == InputTypeLong) {
            /* A hold leaves the app, as it does everywhere else. */
            app->running = false;
        }
    }

    return false;
}
