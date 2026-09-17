#include <furi.h>
#include <stdio.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>

#include "menu_scene.h"
#include "scenes.h"
#include "../../flipper_structs.h"
#include "../../constants.h"
#include "../../hunt.h"
#include "../../clock.h"

enum { MENU_CARE, MENU_HUNT, MENU_EXPEDITION, MENU_INVENTORY, MENU_STATS, MENU_SETTINGS, MENU_HEIR };

static void menu_cb(void *ctx, uint32_t index) {
    struct ApplicationContext *c = ctx;
    view_dispatcher_send_custom_event(c->view_dispatcher, index);
}

void scene_menu_on_enter(void *ctx) {
    struct ApplicationContext *c = ctx;
    Submenu *m = c->menu_module;
    submenu_reset(m);
    submenu_set_header(m, "Dragotchi v" DRAGOTCHI_VERSION);
    struct PersistentGameState *p = &c->game_state->persistent;
    if(p->on_expedition) {
        submenu_add_item(m, "Inventory", MENU_INVENTORY, menu_cb, c);
        submenu_add_item(m, "Stats", MENU_STATS, menu_cb, c);
        submenu_add_item(m, "Settings", MENU_SETTINGS, menu_cb, c);
    } else if(p->stage == DEAD) {
        if(p->eggs_common || p->eggs_rare || p->eggs_storm)
            submenu_add_item(m, "Hatch Heir", MENU_HEIR, menu_cb, c);
        submenu_add_item(m, "Stats", MENU_STATS, menu_cb, c);
        submenu_add_item(m, "Settings", MENU_SETTINGS, menu_cb, c);
    } else {
        submenu_add_item(m, "Care", MENU_CARE, menu_cb, c);
        static char hunt_label[20];
        uint32_t cd = forage_cooldown_remaining(c->game_state, game_now());
        if(cd) {
            snprintf(hunt_label, sizeof(hunt_label), "Hunt (%lus)", (unsigned long)cd);
            submenu_add_item(m, hunt_label, MENU_HUNT, menu_cb, c);
        } else {
            submenu_add_item(m, "Hunt", MENU_HUNT, menu_cb, c);
        }
        submenu_add_item(m, "Expedition", MENU_EXPEDITION, menu_cb, c);
        submenu_add_item(m, "Inventory", MENU_INVENTORY, menu_cb, c);
        submenu_add_item(m, "Stats", MENU_STATS, menu_cb, c);
        submenu_add_item(m, "Settings", MENU_SETTINGS, menu_cb, c);
    }
    view_dispatcher_switch_to_view(c->view_dispatcher, scene_menu);
}

bool scene_menu_on_event(void *ctx, SceneManagerEvent e) {
    struct ApplicationContext *c = ctx;
    if(e.type != SceneManagerEventTypeCustom) return false;
    switch(e.event) {
        case MENU_CARE: scene_manager_next_scene(c->scene_manager, scene_care); break;
        case MENU_EXPEDITION: scene_manager_next_scene(c->scene_manager, scene_expedition); break;
        case MENU_INVENTORY: scene_manager_next_scene(c->scene_manager, scene_inventory); break;
        case MENU_STATS: scene_manager_next_scene(c->scene_manager, scene_status); break;
        case MENU_SETTINGS: scene_manager_next_scene(c->scene_manager, scene_settings); break;
        case MENU_HUNT: {
            struct ThreadsMessage msg = {.type = PROCESS_FORAGE};
            furi_message_queue_put(c->threads_message_queue, &msg, FuriWaitForever);
            scene_manager_search_and_switch_to_previous_scene(c->scene_manager, scene_main);
            break;
        }
        case MENU_HEIR: {
            struct ThreadsMessage msg = {.type = PROCESS_HATCH_HEIR};
            furi_message_queue_put(c->threads_message_queue, &msg, FuriWaitForever);
            scene_manager_search_and_switch_to_previous_scene(c->scene_manager, scene_main);
            break;
        }
    }
    return true;
}

void scene_menu_on_exit(void *ctx) {
    struct ApplicationContext *c = ctx;
    submenu_reset(c->menu_module);
}
