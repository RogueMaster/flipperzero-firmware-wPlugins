#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>

#include "expedition_scene.h"
#include "scenes.h"
#include "../../flipper_structs.h"
#include "../../tuning.h"

static void ex_cb(void *ctx, uint32_t index) {
    struct ApplicationContext *c = ctx;
    view_dispatcher_send_custom_event(c->view_dispatcher, index);
}

void scene_expedition_on_enter(void *ctx) {
    struct ApplicationContext *c = ctx;
    Submenu *m = c->care_module;
    submenu_reset(m);
    submenu_set_header(m, "Expedition");
    submenu_add_item(m, "Short (30m)", 0, ex_cb, c);
    submenu_add_item(m, "Long (2h)", 1, ex_cb, c);
    submenu_add_item(m, "Epic (8h)", 2, ex_cb, c);
    view_dispatcher_switch_to_view(c->view_dispatcher, scene_expedition);
}

bool scene_expedition_on_event(void *ctx, SceneManagerEvent e) {
    struct ApplicationContext *c = ctx;
    if(e.type != SceneManagerEventTypeCustom) return false;
    uint16_t minutes = (e.event == 2) ? EXPED_EPIC_MIN : (e.event == 1) ? EXPED_LONG_MIN : EXPED_SHORT_MIN;
    struct ThreadsMessage msg = {.type = PROCESS_EXPEDITION, .arg = minutes};
    furi_message_queue_put(c->threads_message_queue, &msg, FuriWaitForever);
    scene_manager_search_and_switch_to_previous_scene(c->scene_manager, scene_main);
    return true;
}

void scene_expedition_on_exit(void *ctx) {
    struct ApplicationContext *c = ctx;
    submenu_reset(c->care_module);
}
