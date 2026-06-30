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

#include "about_entity.h"

#include <stddef.h>

#include <furi.h>

#include "src/game.h"

static void
about_update(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(self);
    UNUSED(_entity_context);

    GameContext* game_context = game_manager_game_context_get(manager);

    InputState input = game_manager_input_get(manager);
    if (input.pressed & (GameKeyOk | GameKeyBack | GameKeyLeft)) {
        game_manager_next_level_set(manager, game_context->levels.menu);
    }
}

static void
about_render(Entity* self,
             GameManager* manager,
             Canvas* canvas,
             void* _entity_context)
{
    UNUSED(self);
    UNUSED(manager);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    // Calculate positions
    const size_t logo_size = 10;
    const size_t space = 3;
    size_t title_width =
      logo_size + space + canvas_string_width(canvas, GAME_NAME);
    int32_t logo_x = SCREEN_WIDTH / 2 - title_width / 2;
    int32_t title_x = logo_x + logo_size + space;
    int32_t font_height = canvas_current_font_height(canvas);
    int32_t first_line_y = 17;

    // Draw logo
    AboutContext* entity_context = _entity_context;
    canvas_draw_sprite(canvas, entity_context->logo_sprite, logo_x, 0);

    // Draw game name
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, title_x, 1, AlignLeft, AlignTop, GAME_NAME);

    // Draw authors
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas,
                            64,
                            first_line_y,
                            AlignCenter,
                            AlignTop,
                            "Developed by Ivan Barsukov");
    canvas_draw_str_aligned(canvas,
                            64,
                            first_line_y + font_height,
                            AlignCenter,
                            AlignTop,
                            "Inspired by David Martinez");
    canvas_draw_str_aligned(canvas,
                            64,
                            first_line_y + font_height * 2,
                            AlignCenter,
                            AlignTop,
                            "Graphics by DarKaoz");

    // Draw link
    canvas_draw_str_aligned(
      canvas, 64, 62, AlignCenter, AlignBottom, "github.com/ivanbarsukov");
}

const EntityDescription about_description = {
    .start = NULL,
    .stop = NULL,
    .update = about_update,
    .render = about_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(AboutContext),
};
