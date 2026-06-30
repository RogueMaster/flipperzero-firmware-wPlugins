#include <gui/modules/dialog_ex.h>

#include <pokemon_icons.h>
#include <src/include/pokemon_app.h>
#include <src/include/pokemon_data.h>

#include <src/scenes/include/pokemon_scene.h>

#include <src/views/select_pokemon.h>
#include <src/views/trade.h>

static void pokemon_scene_reset_confirm_dialog_callback(DialogExResult result, void* context) {
    PokemonFap* pokemon_fap = context;

    view_dispatcher_send_custom_event(pokemon_fap->view_dispatcher, result);
}

void pokemon_scene_reset_confirm_on_enter(void* context) {
    PokemonFap* pokemon_fap = context;
    DialogEx* dialog_ex = pokemon_fap->dialog_ex;

    // Clean view
    dialog_ex_reset(pokemon_fap->dialog_ex);

    dialog_ex_set_left_button_text(dialog_ex, "Reset");
    dialog_ex_set_right_button_text(dialog_ex, "Back");
    dialog_ex_set_header(dialog_ex, "Reset Trade State?", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(
        dialog_ex,
        "Trade partner will need to\nleave and re-enter the\nTrade Center!",
        64,
        12,
        AlignCenter,
        AlignTop);
    dialog_ex_set_context(dialog_ex, pokemon_fap);
    dialog_ex_set_result_callback(dialog_ex, pokemon_scene_reset_confirm_dialog_callback);

    view_dispatcher_switch_to_view(pokemon_fap->view_dispatcher, AppViewDialogEx);
}

bool pokemon_scene_reset_confirm_on_event(void* context, SceneManagerEvent event) {
    PokemonFap* pokemon_fap = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DialogExResultLeft:
            trade_reset_connection(pokemon_fap->trade);
            [[fallthrough]];
        case DialogExResultRight:
            scene_manager_previous_scene(pokemon_fap->scene_manager);
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
    }

    return consumed;
}

void pokemon_scene_reset_confirm_on_exit(void* context) {
    UNUSED(context);
}
