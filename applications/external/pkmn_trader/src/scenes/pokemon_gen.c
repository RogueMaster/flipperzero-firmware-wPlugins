#include <src/include/pokemon_app.h>
#include <src/include/pokemon_data.h>
#include <src/include/pokemon_char_encode.h>

#include <src/include/named_list.h>

#include <src/views/trade.h>
#include <src/views/select_pokemon.h>

#include <src/scenes/include/pokemon_scene.h>

#include <src/include/pokemon_attribute.h>

static void scene_change_from_main_cb(void* context, uint32_t index) {
    PokemonFap* pokemon_fap = (PokemonFap*)context;

    /* Set scene state to the current index so we can have that element highlighted when
     * we return. Note that GenI scene is always used as a reference for this in this
     * scene, whether its GenI or GenII.
     */
    scene_manager_set_scene_state(pokemon_fap->scene_manager, PokemonSceneGenITrade, index);

    view_dispatcher_send_custom_event(pokemon_fap->view_dispatcher, index);
}

void pokemon_scene_gen_on_enter(void* context) {
    char buf[32];
    char name_buf[11]; // All name buffers are 11 bytes at most, including term
    PokemonFap* pokemon_fap = (PokemonFap*)context;
    PokemonData* pdata = pokemon_fap->pdata;
    int pkmn_num;
    uint32_t state;

    // Set up trade party struct
    if(!pdata) {
        state = scene_manager_get_scene_state(pokemon_fap->scene_manager, PokemonSceneGenITrade);
        switch(state) {
        case PokemonSceneGenITrade:
            state = GEN_I;
            break;
        case PokemonSceneGenIITrade:
            state = GEN_II;
            break;
        default:
            state = 0;
            break;
        }
        pokemon_fap->pdata = pokemon_data_alloc(state);
        pdata = pokemon_fap->pdata;

        /* Clear the scene state as this is the first entry in to this scene
	 * we definitely want to be completely reset.
	 */
        scene_manager_set_scene_state(pokemon_fap->scene_manager, PokemonSceneGenITrade, 0);

        /* Allocate select and trade views */
        /* Allocates its own view and adds it to the main view_dispatcher */
        pokemon_fap->select =
            select_pokemon_alloc(pdata, pokemon_fap->view_dispatcher, AppViewSelectPokemon);

        // Trade View
        /* Allocates its own view and adds it to the main view_dispatcher */
        pokemon_fap->trade = trade_alloc(
            pdata, pokemon_fap->gblink_handle, pokemon_fap->view_dispatcher, AppViewTrade);
    }

    pkmn_num = pokemon_stat_get(pdata, STAT_NUM, NONE);

    /* Clear the scene state of the Move scene since that is used to set the
     * highlighted menu item.
     * This could be done in move, but move would need its own custom exit handler
     * which is fine but would just waste a few more bytes compared to us handling
     * it here.
     */
    scene_manager_set_scene_state(pokemon_fap->scene_manager, PokemonSceneMove, 0);
    scene_manager_set_scene_state(pokemon_fap->scene_manager, PokemonSceneItemSet, 0);

    submenu_reset(pokemon_fap->submenu);

    snprintf(
        buf, sizeof(buf), "Pokemon:   %s", table_stat_name_get(pdata->pokemon_table, pkmn_num));
    submenu_add_item(
        pokemon_fap->submenu, buf, PokemonSceneSelect, scene_change_from_main_cb, pokemon_fap);

    pokemon_name_get(pdata, STAT_NICKNAME, name_buf, sizeof(name_buf));
    snprintf(buf, sizeof(buf), "Nickname:  %s", name_buf);
    submenu_add_item(
        pokemon_fap->submenu, buf, PokemonSceneNickname, scene_change_from_main_cb, pokemon_fap);

    snprintf(buf, sizeof(buf), "Level:           %d", pokemon_stat_get(pdata, STAT_LEVEL, NONE));
    submenu_add_item(
        pokemon_fap->submenu, buf, PokemonSceneLevel, scene_change_from_main_cb, pokemon_fap);

    if(pdata->gen == GEN_II) {
        snprintf(
            buf,
            sizeof(buf),
            "Held Item:   %s",
            namedlist_name_get_index(
                pdata->item_list, pokemon_stat_get(pdata, STAT_HELD_ITEM, NONE)));
        submenu_add_item(
            pokemon_fap->submenu, buf, PokemonSceneItem, scene_change_from_main_cb, pokemon_fap);
    }

    submenu_add_item(
        pokemon_fap->submenu,
        "Select Moves",
        PokemonSceneMove,
        scene_change_from_main_cb,
        pokemon_fap);

    if(pdata->gen == GEN_I) {
        submenu_add_item(
            pokemon_fap->submenu,
            "Select Types",
            PokemonSceneType,
            scene_change_from_main_cb,
            pokemon_fap);
    }

    submenu_add_item(
        pokemon_fap->submenu,
        namedlist_name_get_index(pdata->stat_list, pokemon_stat_get(pdata, STAT_SEL, NONE)),
        PokemonSceneStats,
        scene_change_from_main_cb,
        pokemon_fap);

    if(pdata->gen == GEN_II) {
        snprintf(
            buf, sizeof(buf), "Shiny:             %s", pokemon_is_shiny(pdata) ? "Yes" : "No");
        submenu_add_item(
            pokemon_fap->submenu, buf, PokemonSceneShiny, scene_change_from_main_cb, pokemon_fap);

        snprintf(buf, sizeof(buf), "Gender:         %s", pokemon_gender_get(pdata));
        submenu_add_item(
            pokemon_fap->submenu, buf, PokemonSceneGender, scene_change_from_main_cb, pokemon_fap);

        snprintf(buf, sizeof(buf), "Pokerus:       %s", pokerus_get_status_str(pdata));
        submenu_add_item(
            pokemon_fap->submenu, buf, PokemonScenePokerus, scene_change_from_main_cb, pokemon_fap);

        if(pokemon_stat_get(pdata, STAT_NUM, NONE) == 0xC8) { // Unown
            snprintf(buf, sizeof(buf), "Unown Form: %c", unown_form_get(pdata));
            submenu_add_item(
                pokemon_fap->submenu,
                buf,
                PokemonSceneUnownForm,
                scene_change_from_main_cb,
                pokemon_fap);
        }
    }

    snprintf(buf, sizeof(buf), "OT ID#:          %05d", pokemon_stat_get(pdata, STAT_OT_ID, NONE));
    submenu_add_item(
        pokemon_fap->submenu, buf, PokemonSceneOTID, scene_change_from_main_cb, pokemon_fap);

    pokemon_name_get(pdata, STAT_OT_NAME, name_buf, sizeof(name_buf));
    snprintf(buf, sizeof(buf), "OT Name:      %s", name_buf);
    submenu_add_item(
        pokemon_fap->submenu, buf, PokemonSceneOTName, scene_change_from_main_cb, pokemon_fap);

    submenu_add_item(
        pokemon_fap->submenu,
        "Trade PKMN",
        PokemonSceneTrade,
        scene_change_from_main_cb,
        pokemon_fap);

    if(trade_connected(pokemon_fap->trade)) {
        submenu_add_item(
            pokemon_fap->submenu,
            "Reset Connection",
            PokemonSceneResetConfirm,
            scene_change_from_main_cb,
            pokemon_fap);
    }

    /* TODO: Add Save pokemon option here */

    /* HACK: No matter what gen were in, we just store the scene state in PokemonSceneGenITrade */
    submenu_set_selected_item(
        pokemon_fap->submenu,
        scene_manager_get_scene_state(pokemon_fap->scene_manager, PokemonSceneGenITrade));

    view_dispatcher_switch_to_view(pokemon_fap->view_dispatcher, AppViewSubmenu);
}

bool pokemon_scene_gen_on_event(void* context, SceneManagerEvent event) {
    PokemonFap* pokemon_fap = context;
    bool consumed = false;
    PokemonData* pdata = pokemon_fap->pdata;
    uint8_t pokemon_num;
    uint8_t gender_ratio;

    /* If the user tries to go back from this scene to main menu, instead
     * shift to the exit confirmation scene. That scene will handle freeing
     * data as well as going back to the main menu.
     */
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(pokemon_fap->scene_manager, PokemonSceneExitConfirm);
        consumed = true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;

        /* Reuse of scenes to allow for using the same functions to set names */
        switch(event.event) {
        case PokemonSceneNickname:
        case PokemonSceneOTName:
        case PokemonSceneUnownForm:
            scene_manager_set_scene_state(
                pokemon_fap->scene_manager, PokemonSceneNickname, event.event);
            break;
        case PokemonSceneLevel:
        case PokemonSceneOTID:
            scene_manager_set_scene_state(
                pokemon_fap->scene_manager, PokemonSceneLevel, event.event);
            break;
        case PokemonSceneGender:
            pokemon_num = pokemon_stat_get(pdata, STAT_NUM, NONE);
            gender_ratio = table_stat_base_get(
                pdata->pokemon_table, pokemon_num, STAT_BASE_GENDER_RATIO, NONE);
            /* If the pokemon's gender is static (always male, always female,
             * or unknown), then don't transition to the gender selection scene.
             */
            if(pokemon_gender_is_static(pdata, gender_ratio)) goto out;
            break;
        }

        scene_manager_next_scene(pokemon_fap->scene_manager, event.event);
    }

out:
    return consumed;
}

void pokemon_scene_gen_on_exit(void* context) {
    UNUSED(context);
}
