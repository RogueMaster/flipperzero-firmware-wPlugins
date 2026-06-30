#include <gui/modules/variable_item_list.h>
#include <furi.h>

#include <src/include/pokemon_app.h>

#include <src/scenes/include/pokemon_scene.h>
#include <src/include/pokemon_attribute.h>

static const char* strains[] = {
    "None",
    "A",
    "B",
    "C",
    "D",
    "",
};

struct pokerus_itemlist {
    VariableItem* strain;
    VariableItem* days;
};

static struct pokerus_itemlist pokerus = {0};
static void select_pokerus_rebuild_list(PokemonFap* pokemon_fap);

static void select_strain_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    PokemonFap* pokemon_fap = variable_item_get_context(item);

    /* Need to set the new text from the mangled index */
    variable_item_set_current_value_text(item, strains[index]);

    /* demangle the index to the value we need to set in trade struct */
    if(index == 0)
        ;
    else if(index == 0x01)
        index = 0x04; // Map this back to the A strain
    else
        index--;

    pokerus_set_strain(pokemon_fap->pdata, index);

    select_pokerus_rebuild_list(pokemon_fap);
    variable_item_list_set_selected_item(pokemon_fap->variable_item_list, 0);
}

static void select_days_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    PokemonFap* pokemon_fap = variable_item_get_context(item);

    pokerus_set_days(pokemon_fap->pdata, index);

    select_pokerus_rebuild_list(pokemon_fap);
    variable_item_list_set_selected_item(pokemon_fap->variable_item_list, 1);
}

static void select_pokerus_rebuild_list(PokemonFap* pokemon_fap) {
    uint8_t strain;
    uint8_t days;
    FuriString* daystring = NULL;

    days = pokemon_stat_get(pokemon_fap->pdata, STAT_POKERUS, NONE);
    strain = (days >> 4);
    days &= 0x0f;

    variable_item_list_reset(pokemon_fap->variable_item_list);

    pokerus.strain = variable_item_list_add(
        pokemon_fap->variable_item_list, "Strain:", 5, select_strain_callback, pokemon_fap);
    pokerus.days = variable_item_list_add(
        pokemon_fap->variable_item_list,
        "Days remain:",
        (strain == 0 ? 0 : 16),
        select_days_callback,
        pokemon_fap);

    /* Strain is a bit weird in that there are only 4 strains, but, a strain of
     * 0 with a days remaining of 0 means the pokemon never had the pokerus.
     * To combat this, we only ever set nibble values 4-7 for the 4 strains,
     * with a value of 0 being specifically reserved for having never had it.
     */
    /* A
     * 0000
     * 0100
     * 1000
     * 1100
     *
     * B
     * 0001
     * 0101
     * 1001
     * 1101
     *
     * C
     * 0010
     * 0110
     * 1010
     * 1110
     *
     * D
     * 0011
     * 0111
     * 1011
     * 1111
     *
     * So, if the whole thing is 0, then it should be considered "Clean"
     * If the lower bits are cleared, but any of the upper bits are set, modify
     * it to be equal to 0100 for our housekeeping.
     * Anything else, we just clear the upper bits and are now in a known good
     * state.
     *
     * So everything is in order, at this point, make a value of 0x04 == 0x1 "A",
     * leave 0x0 as 0, and add 1 to the remaining.
     *
     * When setting, we need to translate this back to the above bit values.
     */
    if(strain == 0)
        ;
    else if(((strain & 0x03) == 0) && ((strain & 0xc0) != 0))
        strain = 0x01; // This would be A
    else {
        strain &= 0x03;
        strain++;
    }

    daystring = furi_string_alloc_printf("%d", days);

    variable_item_set_current_value_index(pokerus.strain, strain);
    variable_item_set_current_value_text(pokerus.strain, strains[strain]);

    variable_item_set_current_value_index(pokerus.days, (strain == 0 ? 0 : days));
    variable_item_set_current_value_text(pokerus.days, furi_string_get_cstr(daystring));
    furi_string_free(daystring);
}

void pokemon_scene_select_pokerus_on_enter(void* context) {
    PokemonFap* pokemon_fap = (PokemonFap*)context;

    select_pokerus_rebuild_list(pokemon_fap);

    view_dispatcher_switch_to_view(pokemon_fap->view_dispatcher, AppViewVariableItem);
}

bool pokemon_scene_select_pokerus_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pokemon_scene_select_pokerus_on_exit(void* context) {
    UNUSED(context);
}
