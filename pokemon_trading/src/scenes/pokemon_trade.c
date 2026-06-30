#include <src/include/pokemon_app.h>

#include <src/scenes/include/pokemon_scene.h>

void pokemon_scene_trade_on_enter(void* context) {
    PokemonFap* pokemon_fap = (PokemonFap*)context;
    // switch to select pokemon scene
    // Note for the future, this might make sense to setup and teardown each view
    // at runtime rather than at the start?
    view_dispatcher_switch_to_view(pokemon_fap->view_dispatcher, AppViewTrade);
}

bool pokemon_scene_trade_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pokemon_scene_trade_on_exit(void* context) {
    UNUSED(context);
}
