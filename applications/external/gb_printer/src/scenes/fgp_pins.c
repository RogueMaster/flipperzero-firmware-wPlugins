// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#include <gui/modules/variable_item_list.h>
#include <furi.h>

#include <src/include/fgp_app.h>
#include <src/scenes/include/fgp_scene.h>

#include <protocols/printer/include/printer_proto.h>
#include <gblink/include/gblink_pinconf.h>

/* When checking for custom pinout, need to pay attention to the actual index of the variable item. as opposed to only rlying on if the pinout is default or not. The get_default function will return true on a custom pinout that is set to one of the named pinouts. Which, when saving, this isn't custom and we can just save the mode name. But here, we need to know if the user selected custom and let them mess with the pins.
 */

/* This must match gblink's enum order */
static const char* named_groups[] = {
    "Original",
    "MLVK2.5",
    "Custom",
    "",
};

static void select_pins_rebuild_list(struct fgp_app* fgp, int mode);

static void select_pins_default_callback(VariableItem* item) {
    struct fgp_app* fgp = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, named_groups[index]);
    gblink_pin_set_default(printer_gblink_handle_get(fgp->printer_handle), index);
    select_pins_rebuild_list(fgp, index);
}

static void select_pins_pin_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    struct fgp_app* fgp = variable_item_get_context(item);
    uint8_t which = variable_item_list_get_selected_item_index(fgp->variable_item_list);
    void* gblink = printer_gblink_handle_get(fgp->printer_handle);
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

    if(index > gblink_pin_get(gblink, pin))
        index = gblink_pin_get_next(index);
    else
        index = gblink_pin_get_prev(index);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, gpio_pins[index].name);
    gblink_pin_set(gblink, pin, index);
}

static void select_pins_rebuild_list(struct fgp_app* fgp, int mode) {
    int pinnum;
    int pinmax = gblink_pin_count_max() + 1;
    VariableItem* item;
    void* gblink = printer_gblink_handle_get(fgp->printer_handle);

    FURI_LOG_D("printer", "rebuild mode %d", mode);
    FURI_LOG_D("printer", "pins count %d, max %d", gpio_pins_count, pinmax);
    variable_item_list_reset(fgp->variable_item_list);

    item = variable_item_list_add(
        fgp->variable_item_list, "Mode", PINOUT_COUNT + 1, select_pins_default_callback, fgp);
    variable_item_set_current_value_index(item, mode);
    variable_item_set_current_value_text(item, named_groups[mode]);

    item = variable_item_list_add(
        fgp->variable_item_list,
        "SI:",
        (mode < PINOUT_COUNT) ? 1 : pinmax,
        select_pins_pin_callback,
        fgp);
    pinnum = gblink_pin_get(gblink, PIN_SERIN);
    variable_item_set_current_value_index(item, (mode < PINOUT_COUNT) ? 0 : pinnum);
    variable_item_set_current_value_text(item, gpio_pins[pinnum].name);

    item = variable_item_list_add(
        fgp->variable_item_list,
        "SO:",
        (mode < PINOUT_COUNT) ? 1 : pinmax,
        select_pins_pin_callback,
        fgp);
    pinnum = gblink_pin_get(gblink, PIN_SEROUT);
    variable_item_set_current_value_index(item, (mode < PINOUT_COUNT) ? 0 : pinnum);
    variable_item_set_current_value_text(item, gpio_pins[pinnum].name);

    item = variable_item_list_add(
        fgp->variable_item_list,
        "CLK:",
        (mode < PINOUT_COUNT) ? 1 : pinmax,
        select_pins_pin_callback,
        fgp);
    pinnum = gblink_pin_get(gblink, PIN_CLK);
    variable_item_set_current_value_index(item, (mode < PINOUT_COUNT) ? 0 : pinnum);
    variable_item_set_current_value_text(item, gpio_pins[pinnum].name);
}

void fgp_scene_select_pins_on_enter(void* context) {
    struct fgp_app* fgp = (struct fgp_app*)context;
    int def_mode = gblink_pin_get_default(printer_gblink_handle_get(fgp->printer_handle));
    FURI_LOG_D("printer", "def_mode %d", def_mode);

    select_pins_rebuild_list(fgp, (def_mode < 0) ? PINOUT_COUNT : def_mode);

    variable_item_list_set_selected_item(fgp->variable_item_list, 0);

    view_dispatcher_switch_to_view(fgp->view_dispatcher, fgpViewVariableItemList);
}

bool fgp_scene_select_pins_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void fgp_scene_select_pins_on_exit(void* context) {
    struct fgp_app* fgp = context;

    gblink_pinconf_save(printer_gblink_handle_get(fgp->printer_handle));
}
