#ifndef POKEMON_SCENE_H
#define POKEMON_SCENE_H

#pragma once

#include <gui/scene_manager.h>

// Generate scene id and total number
#define ADD_SCENE(prefix, name, id) PokemonScene##id,
typedef enum {
#include "pokemon_scene_config.h"
    PokemonSceneNum,
    /* Magic number to send on an event to search through scene history and
     * change to a previous scene.
     */
    PokemonSceneSearch = (1 << 30),
    /* Magic number to send on an event to trigger going back a scene */
    PokemonSceneBack = (1 << 31),
} PokemonScene;
#undef ADD_SCENE

extern const SceneManagerHandlers pokemon_scene_handlers;

/* Disable redundant declarations for this.
 * Normally not something we would want, however, this codebase does reuse
 * functions (using different scene IDs) to handle what would otherwise be
 * the same (or very similar) code. e.g. OT Name, nickname, and unown form use
 * the same name select scene with a different ID. Similar for OT ID and level.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "pokemon_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "pokemon_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "pokemon_scene_config.h"
#undef ADD_SCENE

#pragma GCC diagnostic pop

#endif // POKEMON_SCENE_H
