// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#include <gui/modules/submenu.h>
#include <src/include/fgp_app.h>
#include <src/scenes/include/fgp_scene.h>

static void scene_change_from_main_cb(void* context, uint32_t index) {
    struct fgp_app* fgp = context;

    /* Set scene state to the current index so we can have that element highlighted when
	* we return.
	*/
    scene_manager_set_scene_state(fgp->scene_manager, fgpSceneMenu, index);

    view_dispatcher_send_custom_event(fgp->view_dispatcher, index);
}

void fgp_scene_menu_on_enter(void* context) {
    struct fgp_app* fgp = context;

    submenu_reset(fgp->submenu);
    submenu_set_header(fgp->submenu, "Game Boy Printer");

    /* Init config variables here */
    fgp->palette_idx = 0;

    submenu_add_item(
        fgp->submenu, "Receive from GB", fgpSceneReceiveConf, scene_change_from_main_cb, fgp);

    submenu_add_item(
        fgp->submenu, "Select Pinout", fgpSceneSelectPins, scene_change_from_main_cb, fgp);

    submenu_set_selected_item(
        fgp->submenu, scene_manager_get_scene_state(fgp->scene_manager, fgpSceneMenu));

    view_dispatcher_switch_to_view(fgp->view_dispatcher, fgpViewSubmenu);
}

bool fgp_scene_menu_on_event(void* context, SceneManagerEvent event) {
    struct fgp_app* fgp = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_next_scene(fgp->scene_manager, event.event);
        consumed = true;
    }

    return consumed;
}

void fgp_scene_menu_on_exit(void* context) {
    UNUSED(context);
}
