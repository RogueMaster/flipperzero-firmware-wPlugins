// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#include <gui/modules/variable_item_list.h>
#include <src/include/fgp_app.h>
#include <src/scenes/include/fgp_scene.h>

#include <src/include/fgp_palette.h>

static const char* const list_text[] = {
    "Save bin:",
    "Save hdr+bin:",
    "Save PNG:",
    "PNG Palette:",
    "Receive!",
};

static const char* const yes_no_text[] = {
    "No",
    "Yes",
};

static void save_binary(VariableItem* item) {
    struct fgp_app* fgp = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, yes_no_text[index]);
    fgp->options &= ~OPT_SAVE_BIN;
    if(index) fgp->options |= OPT_SAVE_BIN;
}

static void save_binary_w_header(VariableItem* item) {
    struct fgp_app* fgp = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, yes_no_text[index]);
    fgp->options &= ~OPT_SAVE_BIN_HDR;
    if(index) fgp->options |= OPT_SAVE_BIN_HDR;
}

static void save_png(VariableItem* item) {
    struct fgp_app* fgp = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, yes_no_text[index]);
    fgp->options &= ~OPT_SAVE_PNG;
    if(index) fgp->options |= OPT_SAVE_PNG;
}

static void set_palette(VariableItem* item) {
    struct fgp_app* fgp = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, palette_name_get(index));
    fgp->palette_idx = index;
}

static void enter_callback(void* context, uint32_t index) {
    struct fgp_app* fgp = context;

    scene_manager_set_scene_state(fgp->scene_manager, fgpSceneReceiveConf, index);
    /* When the user presses enter on the last option in the list, that is
	 * the cue to switch to the next scene.
	 */
    if((index == COUNT_OF(list_text) - 1) && (fgp->options & RECV_OPTS))
        view_dispatcher_send_custom_event(fgp->view_dispatcher, 0);
}

void fgp_scene_receive_conf_on_enter(void* context) {
    struct fgp_app* fgp = context;
    VariableItem* item;

    variable_item_list_reset(fgp->variable_item_list);
    variable_item_list_set_selected_item(
        fgp->variable_item_list,
        scene_manager_get_scene_state(fgp->scene_manager, fgpSceneReceiveConf));

    /* Set default options */
    /* TODO: Save any of these to SD as settings? */
    fgp->options = (OPT_SAVE_PNG | OPT_SAVE_BIN);

    item = variable_item_list_add(
        fgp->variable_item_list, list_text[0], COUNT_OF(yes_no_text), save_binary, fgp);
    variable_item_set_current_value_index(item, !!(fgp->options & OPT_SAVE_BIN));
    variable_item_set_current_value_text(item, yes_no_text[(!!(fgp->options & OPT_SAVE_BIN))]);

    item = variable_item_list_add(
        fgp->variable_item_list, list_text[1], COUNT_OF(yes_no_text), save_binary_w_header, fgp);
    variable_item_set_current_value_index(item, !!(fgp->options & OPT_SAVE_BIN_HDR));
    variable_item_set_current_value_text(item, yes_no_text[(!!(fgp->options & OPT_SAVE_BIN_HDR))]);

    item = variable_item_list_add(
        fgp->variable_item_list, list_text[2], COUNT_OF(yes_no_text), save_png, fgp);
    variable_item_set_current_value_index(item, !!(fgp->options & OPT_SAVE_PNG));
    variable_item_set_current_value_text(item, yes_no_text[(!!(fgp->options & OPT_SAVE_PNG))]);

    item = variable_item_list_add(
        fgp->variable_item_list, list_text[3], palette_count_get(), set_palette, fgp);
    variable_item_set_current_value_index(item, fgp->palette_idx);
    variable_item_set_current_value_text(item, palette_name_get(fgp->palette_idx));

    item = variable_item_list_add(fgp->variable_item_list, list_text[4], 0, NULL, fgp);

    variable_item_list_set_enter_callback(fgp->variable_item_list, enter_callback, fgp);

    view_dispatcher_switch_to_view(fgp->view_dispatcher, fgpViewVariableItemList);
}

bool fgp_scene_receive_conf_on_event(void* context, SceneManagerEvent event) {
    struct fgp_app* fgp = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 0) view_dispatcher_switch_to_view(fgp->view_dispatcher, fgpViewReceive);
        consumed = true;
    }
    return consumed;
}

void fgp_scene_receive_conf_on_exit(void* context) {
    UNUSED(context);
}
