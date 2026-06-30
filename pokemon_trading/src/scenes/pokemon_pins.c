#include <gui/modules/variable_item_list.h>
#include <furi.h>

#include <src/include/pokemon_app.h>

#include <src/scenes/include/pokemon_scene.h>

#include <gblink/include/gblink_pinconf.h>
#include <gblink/include/gblink.h>

/* This must match gblink's enum order */
static const char* named_groups[] = {
    "Original",
    "MLVK2.5",
    "Custom",
    "",
};

static void select_pins_rebuild_list(PokemonFap* pokemon_fap, int mode);

static void select_pins_default_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    PokemonFap* pokemon_fap = variable_item_get_context(item);

    variable_item_set_current_value_text(item, named_groups[index]);
    gblink_pin_set_default(pokemon_fap->gblink_handle, index);
    select_pins_rebuild_list(pokemon_fap, index);
}

static void select_pins_pin_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    PokemonFap* pokemon_fap = variable_item_get_context(item);
    uint8_t which = variable_item_list_get_selected_item_index(pokemon_fap->variable_item_list);
    gblink_bus_pins pin;

    switch(which) {
    case 1: // SI
        pin = PIN_SERIN;
        break;
    case 2: // SO
        pin = PIN_SEROUT;
        break;
    case 3: // CLK
        pin = PIN_CLK;
        break;
    default:
        furi_crash();
        break;
    }

    if(index > gblink_pin_get(pokemon_fap->gblink_handle, pin))
        index = gblink_pin_get_next(index);
    else
        index = gblink_pin_get_prev(index);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, gpio_pins[index].name);
    gblink_pin_set(pokemon_fap->gblink_handle, pin, index);
}

static void select_pins_rebuild_list(PokemonFap* pokemon_fap, int mode) {
    int pinnum;
    int pinmax = gblink_pin_count_max() + 1;
    VariableItem* item;

    variable_item_list_reset(pokemon_fap->variable_item_list);

    item = variable_item_list_add(
        pokemon_fap->variable_item_list,
        "Mode",
        PINOUT_COUNT + 1,
        select_pins_default_callback,
        pokemon_fap);
    variable_item_set_current_value_index(item, mode);
    variable_item_set_current_value_text(item, named_groups[mode]);

    item = variable_item_list_add(
        pokemon_fap->variable_item_list,
        "SI:",
        (mode < PINOUT_COUNT) ? 1 : pinmax,
        select_pins_pin_callback,
        pokemon_fap);
    pinnum = gblink_pin_get(pokemon_fap->gblink_handle, PIN_SERIN);
    variable_item_set_current_value_index(item, (mode < PINOUT_COUNT) ? 0 : pinnum);
    variable_item_set_current_value_text(item, gpio_pins[pinnum].name);

    item = variable_item_list_add(
        pokemon_fap->variable_item_list,
        "SO:",
        (mode < PINOUT_COUNT) ? 1 : pinmax,
        select_pins_pin_callback,
        pokemon_fap);
    pinnum = gblink_pin_get(pokemon_fap->gblink_handle, PIN_SEROUT);
    variable_item_set_current_value_index(item, (mode < PINOUT_COUNT) ? 0 : pinnum);
    variable_item_set_current_value_text(item, gpio_pins[pinnum].name);

    item = variable_item_list_add(
        pokemon_fap->variable_item_list,
        "CLK:",
        (mode < PINOUT_COUNT) ? 1 : pinmax,
        select_pins_pin_callback,
        pokemon_fap);
    pinnum = gblink_pin_get(pokemon_fap->gblink_handle, PIN_CLK);
    variable_item_set_current_value_index(item, (mode < PINOUT_COUNT) ? 0 : pinnum);
    variable_item_set_current_value_text(item, gpio_pins[pinnum].name);
}

void pokemon_scene_select_pins_on_enter(void* context) {
    PokemonFap* pokemon_fap = (PokemonFap*)context;
    int def_mode = gblink_pin_get_default(pokemon_fap->gblink_handle);

    select_pins_rebuild_list(pokemon_fap, (def_mode < 0) ? PINOUT_COUNT : def_mode);

    view_dispatcher_switch_to_view(pokemon_fap->view_dispatcher, AppViewVariableItem);
}

bool pokemon_scene_select_pins_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pokemon_scene_select_pins_on_exit(void* context) {
    PokemonFap* pokemon_fap = context;

    gblink_pinconf_save(pokemon_fap->gblink_handle);
}
