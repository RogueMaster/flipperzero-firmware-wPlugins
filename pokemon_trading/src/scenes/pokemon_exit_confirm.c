#include <gui/modules/dialog_ex.h>

#include <pokemon_icons.h>
#include <src/include/pokemon_app.h>
#include <src/include/pokemon_data.h>

#include <src/scenes/include/pokemon_scene.h>

#include <src/views/select_pokemon.h>
#include <src/views/trade.h>

static void pokemon_scene_exit_confirm_dialog_callback(DialogExResult result, void* context) {
    PokemonFap* pokemon_fap = context;

    view_dispatcher_send_custom_event(pokemon_fap->view_dispatcher, result);
}

void pokemon_scene_exit_confirm_on_enter(void* context) {
    PokemonFap* pokemon_fap = context;
    DialogEx* dialog_ex = pokemon_fap->dialog_ex;

    // Clean view
    dialog_ex_reset(pokemon_fap->dialog_ex);

    dialog_ex_set_left_button_text(dialog_ex, "Exit");
    dialog_ex_set_right_button_text(dialog_ex, "Stay");
    dialog_ex_set_header(dialog_ex, "Exit to Main Menu?", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(
        dialog_ex,
        "Current configuration and/or\ntrade status will be lost!",
        64,
        12,
        AlignCenter,
        AlignTop);
    dialog_ex_set_icon(dialog_ex, 44, 32, &I_surprised_pika);
    dialog_ex_set_context(dialog_ex, pokemon_fap);
    dialog_ex_set_result_callback(dialog_ex, pokemon_scene_exit_confirm_dialog_callback);

    view_dispatcher_switch_to_view(pokemon_fap->view_dispatcher, AppViewDialogEx);
}

bool pokemon_scene_exit_confirm_on_event(void* context, SceneManagerEvent event) {
    PokemonFap* pokemon_fap = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultRight) {
            consumed = scene_manager_previous_scene(pokemon_fap->scene_manager);
        } else if(event.event == DialogExResultLeft) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                pokemon_fap->scene_manager, PokemonSceneMainMenu);
            /* NOTE: The above should never fail */
            furi_check(consumed);

            /* NOTE: The following might need to change when adding more 
	     * feature support.
	     */
            /* Clean up PokemonData */
            pokemon_data_free(pokemon_fap->pdata);

            // Free views
            /* These each remove themselves from the view_dispatcher */
            select_pokemon_free(
                pokemon_fap->view_dispatcher, AppViewSelectPokemon, pokemon_fap->select);
            trade_free(pokemon_fap->view_dispatcher, AppViewTrade, pokemon_fap->trade);

            pokemon_fap->pdata = NULL;
            pokemon_fap->select = NULL;
            pokemon_fap->trade = NULL;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
    }

    return consumed;
}

void pokemon_scene_exit_confirm_on_exit(void* context) {
    UNUSED(context);
}
