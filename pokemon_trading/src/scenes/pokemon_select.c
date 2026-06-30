#include <src/include/pokemon_app.h>

#include <src/scenes/include/pokemon_scene.h>

void pokemon_scene_select_pokemon_on_enter(void* context) {
    PokemonFap* pokemon_fap = (PokemonFap*)context;
    // switch to select pokemon scene
    // Note for the future, this might make sense to setup and teardown each view
    // at runtime rather than at the start of the whole application
    view_dispatcher_switch_to_view(pokemon_fap->view_dispatcher, AppViewSelectPokemon);
}

bool pokemon_scene_select_pokemon_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    PokemonFap* pokemon_fap = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event & PokemonSceneBack) {
        scene_manager_previous_scene(pokemon_fap->scene_manager);
        consumed = true;
    }

    return consumed;
}

void pokemon_scene_select_pokemon_on_exit(void* context) {
    UNUSED(context);
}
