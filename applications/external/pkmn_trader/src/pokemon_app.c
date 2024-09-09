#include <furi_hal_light.h>
#include <pokemon_icons.h>

#include <src/include/pokemon_app.h>
#include <src/include/pokemon_data.h>
#include <src/views/trade.h>
#include <src/views/select_pokemon.h>
#include <src/include/pokemon_char_encode.h>

#include <src/scenes/include/pokemon_scene.h>

bool pokemon_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    PokemonFap* pokemon_fap = context;

    return scene_manager_handle_custom_event(pokemon_fap->scene_manager, event);
}

bool pokemon_back_event_callback(void* context) {
    furi_assert(context);
    PokemonFap* pokemon_fap = context;
    return scene_manager_handle_back_event(pokemon_fap->scene_manager);
}

PokemonFap* pokemon_alloc() {
    PokemonFap* pokemon_fap = (PokemonFap*)malloc(sizeof(PokemonFap));
    ViewDispatcher* view_dispatcher = NULL;

    // View dispatcher
    view_dispatcher = view_dispatcher_alloc();
    pokemon_fap->view_dispatcher = view_dispatcher;

    view_dispatcher_set_event_callback_context(view_dispatcher, pokemon_fap);
    view_dispatcher_set_custom_event_callback(view_dispatcher, pokemon_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(view_dispatcher, pokemon_back_event_callback);
    view_dispatcher_attach_to_gui(
        view_dispatcher, (Gui*)furi_record_open(RECORD_GUI), ViewDispatcherTypeFullscreen);

    // Set up pinout defaults
    memcpy(&pokemon_fap->pins, &common_pinouts[PINOUT_ORIGINAL], sizeof(struct gblink_pins));

    // Text input
    pokemon_fap->text_input = text_input_alloc();
    view_dispatcher_add_view(
        view_dispatcher, AppViewTextInput, text_input_get_view(pokemon_fap->text_input));

    // Submenu
    pokemon_fap->submenu = submenu_alloc();
    view_dispatcher_add_view(
        view_dispatcher, AppViewSubmenu, submenu_get_view(pokemon_fap->submenu));

    // Variable Item List
    pokemon_fap->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        view_dispatcher,
        AppViewVariableItem,
        variable_item_list_get_view(pokemon_fap->variable_item_list));

    // DialogEx
    pokemon_fap->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        view_dispatcher, AppViewDialogEx, dialog_ex_get_view(pokemon_fap->dialog_ex));

    // Scene manager
    pokemon_fap->scene_manager = scene_manager_alloc(&pokemon_scene_handlers, pokemon_fap);
    scene_manager_next_scene(pokemon_fap->scene_manager, PokemonSceneMainMenu);

    return pokemon_fap;
}

void free_app(PokemonFap* pokemon_fap) {
    furi_assert(pokemon_fap);

    // Submenu
    submenu_free(pokemon_fap->submenu);
    view_dispatcher_remove_view(pokemon_fap->view_dispatcher, AppViewSubmenu);

    text_input_free(pokemon_fap->text_input);
    view_dispatcher_remove_view(pokemon_fap->view_dispatcher, AppViewTextInput);

    variable_item_list_free(pokemon_fap->variable_item_list);
    view_dispatcher_remove_view(pokemon_fap->view_dispatcher, AppViewVariableItem);

    dialog_ex_free(pokemon_fap->dialog_ex);
    view_dispatcher_remove_view(pokemon_fap->view_dispatcher, AppViewDialogEx);

    view_dispatcher_free(pokemon_fap->view_dispatcher);

    // Free scenes
    scene_manager_free(pokemon_fap->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);

    // Free rest
    free(pokemon_fap);
    pokemon_fap = NULL;
}

int32_t pokemon_app(void* p) {
    UNUSED(p);
    PokemonFap* pokemon_fap = pokemon_alloc();

    furi_hal_light_set(LightRed, 0x00);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue, 0x00);

    //switch view and run dispatcher
    view_dispatcher_run(pokemon_fap->view_dispatcher);

    // Free resources
    free_app(pokemon_fap);

    return 0;
}
