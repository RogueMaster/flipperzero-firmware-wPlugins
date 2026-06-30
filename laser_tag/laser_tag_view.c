#include "laser_tag_view.h"
#include <furi.h>
#include <gui/elements.h>

struct LaserTagView {
    View* view;
};

typedef struct {
    LaserTagTeam team;
    uint8_t health;
    uint16_t ammo;
    uint32_t game_time;
    bool game_over;
} LaserTagViewModel;

static void laser_tag_view_draw_callback(Canvas* canvas, void* model) {
    LaserTagViewModel* m = model;
    furi_assert(m);
    furi_assert(canvas);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_str_aligned(
        canvas, 5, 10, AlignLeft, AlignBottom, m->team == TeamRed ? "Team: Red" : "Team: Blue");

    canvas_draw_str_aligned(canvas, 5, 25, AlignLeft, AlignBottom, "Health:");
    canvas_draw_frame(canvas, 55, 20, 60, 10);
    canvas_draw_box(canvas, 56, 21, (58 * m->health) / 100, 8);

    canvas_draw_str_aligned(canvas, 5, 40, AlignLeft, AlignBottom, "Ammo:");
    canvas_draw_frame(canvas, 55, 35, 60, 10);
    canvas_draw_box(canvas, 56, 36, (58 * m->ammo) / 100, 8);

    if(m->ammo == 0) {
        canvas_draw_str_aligned(canvas, 5, 55, AlignLeft, AlignBottom, "Press 'Down' to Reload");
    }

    uint32_t minutes = m->game_time / 60;
    uint32_t seconds = m->game_time % 60;
    FuriString* str = furi_string_alloc_printf("%02ld:%02ld", minutes, seconds);
    canvas_draw_str_aligned(canvas, 5, 60, AlignLeft, AlignBottom, furi_string_get_cstr(str));

    if(m->game_over) {
        canvas_draw_str_aligned(canvas, 5, 75, AlignLeft, AlignBottom, "GAME OVER");
    }

    furi_string_free(str);
}

static bool laser_tag_view_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

LaserTagView* laser_tag_view_alloc() {
    LaserTagView* laser_tag_view = malloc(sizeof(LaserTagView));
    if(!laser_tag_view) {
        return NULL;
    }

    laser_tag_view->view = view_alloc();
    if(!laser_tag_view->view) {
        free(laser_tag_view);
        return NULL;
    }

    view_set_context(laser_tag_view->view, laser_tag_view);
    view_allocate_model(laser_tag_view->view, ViewModelTypeLocking, sizeof(LaserTagViewModel));
    view_set_draw_callback(laser_tag_view->view, laser_tag_view_draw_callback);
    view_set_input_callback(laser_tag_view->view, laser_tag_view_input_callback);

    return laser_tag_view;
}

void laser_tag_view_free(LaserTagView* laser_tag_view) {
    if(!laser_tag_view) return;
    if(laser_tag_view->view) {
        view_free(laser_tag_view->view);
    }
    free(laser_tag_view);
}

void laser_tag_view_draw(View* view, Canvas* canvas) {
    furi_assert(view);
    furi_assert(canvas);
    LaserTagViewModel* model = view_get_model(view);
    laser_tag_view_draw_callback(canvas, model);
    view_commit_model(view, false);
}

View* laser_tag_view_get_view(LaserTagView* laser_tag_view) {
    furi_assert(laser_tag_view);
    return laser_tag_view->view;
}

void laser_tag_view_update(LaserTagView* laser_tag_view, GameState* game_state) {
    furi_assert(laser_tag_view);
    furi_assert(game_state);

    with_view_model(
        laser_tag_view->view,
        LaserTagViewModel * model,
        {
            model->team = game_state_get_team(game_state);
            model->health = game_state_get_health(game_state);
            model->ammo = game_state_get_ammo(game_state);
            model->game_time = game_state_get_time(game_state);
            model->game_over = game_state_is_game_over(game_state);
        },
        true);
}
