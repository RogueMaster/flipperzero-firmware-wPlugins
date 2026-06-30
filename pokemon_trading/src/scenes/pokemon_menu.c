#include <src/include/pokemon_app.h>
#include <src/include/pokemon_data.h>
#include <src/include/pokemon_char_encode.h>

#include <src/scenes/include/pokemon_scene.h>

static void scene_change_from_main_cb(void* context, uint32_t index) {
    PokemonFap* pokemon_fap = (PokemonFap*)context;

    /* The same trade scene is used for both gen i and ii. Set the real index to
     * scene's state.
     */
    scene_manager_set_scene_state(pokemon_fap->scene_manager, PokemonSceneGenITrade, index);

    /* Set scene state to the current index so we can have that element highlighted when
     * we return.
     */
    scene_manager_set_scene_state(pokemon_fap->scene_manager, PokemonSceneMainMenu, index);

    view_dispatcher_send_custom_event(pokemon_fap->view_dispatcher, index);
}

void pokemon_scene_main_menu_on_enter(void* context) {
    PokemonFap* pokemon_fap = (PokemonFap*)context;

    submenu_reset(pokemon_fap->submenu);
    submenu_set_header(pokemon_fap->submenu, "Pokemon Trade Tool");

    submenu_add_item(
        pokemon_fap->submenu,
        "Gen I    (R/B/Y non-JPN)",
        PokemonSceneGenITrade,
        scene_change_from_main_cb,
        pokemon_fap);
    submenu_add_item(
        pokemon_fap->submenu,
        "Gen II   (G/S/C non-JPN)",
        PokemonSceneGenIITrade,
        scene_change_from_main_cb,
        pokemon_fap);
    submenu_add_item(
        pokemon_fap->submenu,
        "Select EXT Pinout",
        PokemonScenePins,
        scene_change_from_main_cb,
        pokemon_fap);

    submenu_set_selected_item(
        pokemon_fap->submenu,
        scene_manager_get_scene_state(pokemon_fap->scene_manager, PokemonSceneMainMenu));

    view_dispatcher_switch_to_view(pokemon_fap->view_dispatcher, AppViewSubmenu);
}

bool pokemon_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    PokemonFap* pokemon_fap = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_next_scene(pokemon_fap->scene_manager, event.event);
        consumed = true;
    }

    return consumed;
}

void pokemon_scene_main_menu_on_exit(void* context) {
    UNUSED(context);
}
