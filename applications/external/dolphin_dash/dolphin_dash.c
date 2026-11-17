#include "dash_game.h"
#include <dolphin_dash_icons.h>
#include <furi.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <stdio.h>

static void draw_button(Canvas* canvas, const char* label) {
    canvas_draw_rbox(canvas, 34, 50, 60, 13, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_reef(Canvas* canvas, const Reef* reef) {
    int y = 21 + 16 * reef->lane;
    if(reef->x - RockRadius < 128) {
        canvas_draw_icon(canvas, reef->x - RockRadius, y - 3, &I_rock);
    }
    int x = reef->x - BubbleLead;
    if(reef->has_bubble && x >= -BubbleRadius && x < 128 + BubbleRadius) {
        canvas_draw_circle(canvas, x, y, BubbleRadius);
        canvas_draw_dot(canvas, x - 1, y - 1);
    }
}

static void draw(Canvas* canvas, void* model) {
    const DashGame* game = model;
    char text[32];
    canvas_clear(canvas);
    canvas_set_bitmap_mode(canvas, true);
    canvas_set_font(canvas, FontPrimary);
    if(game->phase == DashTitle) {
        canvas_draw_icon(canvas, 6, 6, &I_swimmer);
        canvas_draw_str(canvas, 30, 15, "Dolphin Dash");
        canvas_draw_line(canvas, 3, 20, 124, 20);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 32, AlignCenter, AlignBottom, "Up / Down to dodge rocks");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, "Catch bubbles for +3");
        draw_button(canvas, "OK  Dive");
    } else if(game->phase == DashOver) {
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "Wipeout!");
        canvas_draw_line(canvas, 16, 18, 111, 18);
        snprintf(text, sizeof(text), "Score  %u", game->score);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignBottom, text);
        canvas_set_font(canvas, FontSecondary);
        snprintf(text, sizeof(text), "Session best  %u", game->best);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignBottom, text);
        draw_button(canvas, "OK  Retry");
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 9, "Dolphin Dash");
        snprintf(text, sizeof(text), "%u", game->score);
        canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, text);
        canvas_draw_line(canvas, 0, 12, 127, 12);
        for(int x = 8 - (int)(game->frame % 8); x < 128; x += 8) {
            canvas_draw_dot(canvas, x, 29);
            canvas_draw_dot(canvas, x, 45);
        }
        int y = 21 + 16 * game->lane;
        canvas_draw_icon(canvas, 10, y - 5, &I_swimmer);
        int wake = (int)(game->frame / 2 % 3);
        canvas_draw_line(canvas, 4 - wake, y, 7 - wake, y);
        for(unsigned i = 0; i < ReefCount; i++)
            draw_reef(canvas, &game->reefs[i]);
    }
}

static unsigned next_lane(void) {
    return furi_hal_random_get() % LaneCount;
}

static void tick(void* context) {
    View* view = context;
    DashGame* game = view_get_model(view);
    bool swimming = game->phase == DashSwimming;
    if(swimming) dash_tick(game, next_lane);
    view_commit_model(view, swimming);
}

static bool input(InputEvent* event, void* context) {
    if(event->type != InputTypePress || event->key == InputKeyBack) return false;
    View* view = context;
    DashGame* game = view_get_model(view);
    if(event->key == InputKeyOk && game->phase != DashSwimming) {
        dash_start(game, furi_hal_random_get());
    } else if(event->key == InputKeyUp || event->key == InputKeyDown) {
        dash_move(game, event->key == InputKeyUp ? -1 : 1);
    }
    view_commit_model(view, true);
    return true;
}

static uint32_t exit_view(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

int32_t dolphin_dash_app(void* arg) {
    UNUSED(arg);
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewDispatcher* dispatcher = view_dispatcher_alloc();
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(DashGame));
    view_set_context(view, view);
    view_set_draw_callback(view, draw);
    view_set_input_callback(view, input);
    view_set_previous_callback(view, exit_view);
    view_dispatcher_set_event_callback_context(dispatcher, view);
    view_dispatcher_set_tick_event_callback(dispatcher, tick, furi_ms_to_ticks(50));
    view_dispatcher_add_view(dispatcher, 0, view);
    view_dispatcher_attach_to_gui(dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(dispatcher, 0);
    view_dispatcher_run(dispatcher);
    view_dispatcher_remove_view(dispatcher, 0);
    view_free(view);
    view_dispatcher_free(dispatcher);
    furi_record_close(RECORD_GUI);
    return 0;
}
