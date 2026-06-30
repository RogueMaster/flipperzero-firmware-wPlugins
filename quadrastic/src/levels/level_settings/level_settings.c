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

#include "level_settings.h"

#include <stddef.h>

#include <gui/modules/variable_item_list.h>

#include "src/game.h"
#include "src/game_settings.h"

#include "src/gui_bridge/view_module_descriptions.h"
#include "src/gui_bridge/view_module_entity.h"

const char* const difficulty_text[DifficultyCount] = {
    "Easy",
    "Normal",
    "Hard",
    "Insane",
};

const char* const state_text[StateCount] = {
    "OFF",
    "ON",
};

static bool
back_callback(void* context)
{
    GameManager* manager = context;
    GameContext* game_context = game_manager_game_context_get(manager);
    game_save_settings(game_context);
    game_manager_next_level_set(manager, game_context->levels.menu);
    return true;
}

static void
difficulty_change_callback(VariableItem* item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, difficulty_text[index]);

    GameManager* game_manager = variable_item_get_context(item);
    GameContext* game_context = game_manager_game_context_get(game_manager);
    game_context->difficulty = index;
}

static void
state_change_callback(VariableItem* item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, state_text[index]);

    State* state = variable_item_get_context(item);
    *state = (State)index;
}

static void
level_settings_alloc(Level* level, GameManager* manager, void* context)
{
    UNUSED(context);

    Entity* entity =
      view_module_add_to_level(level, manager, &variable_item_list_description);
    view_module_set_back_callback(entity, back_callback, manager);

    GameContext* game_context = game_manager_game_context_get(manager);
    VariableItemList* variable_item_list = view_module_get_module(entity);
    VariableItem* item;

    // Add difficulty
    item = variable_item_list_add(variable_item_list,
                                  "Difficulty",
                                  DifficultyCount,
                                  difficulty_change_callback,
                                  manager);
    variable_item_set_current_value_index(item, game_context->difficulty);
    variable_item_set_current_value_text(
      item, difficulty_text[game_context->difficulty]);

    // Add sound
    item = variable_item_list_add(variable_item_list,
                                  "Sound",
                                  StateCount,
                                  state_change_callback,
                                  &game_context->sound);
    variable_item_set_current_value_index(item, game_context->sound);
    variable_item_set_current_value_text(item, state_text[game_context->sound]);

    // Add vibro
    item = variable_item_list_add(variable_item_list,
                                  "Vibro",
                                  StateCount,
                                  state_change_callback,
                                  &game_context->vibro);
    variable_item_set_current_value_index(item, game_context->vibro);
    variable_item_set_current_value_text(item, state_text[game_context->vibro]);

    // Add led
    item = variable_item_list_add(variable_item_list,
                                  "LED",
                                  StateCount,
                                  state_change_callback,
                                  &game_context->led);
    variable_item_set_current_value_index(item, game_context->led);
    variable_item_set_current_value_text(item, state_text[game_context->led]);

    FURI_LOG_D(GAME_NAME, "Settings level allocated");
}

static void
level_settings_start(Level* level, GameManager* manager, void* context)
{
    UNUSED(level);
    UNUSED(manager);
    UNUSED(context);

    Entity* entity = level_entity_get(level, &view_module_description, 0);

    VariableItemList* variable_item_list = view_module_get_module(entity);
    variable_item_list_set_selected_item(variable_item_list, 0);

    FURI_LOG_D(GAME_NAME, "Settings level started");
}

const LevelBehaviour level_settings = {
    .alloc = level_settings_alloc,
    .free = NULL,
    .start = level_settings_start,
    .stop = NULL,
    .context_size = 0,
};
