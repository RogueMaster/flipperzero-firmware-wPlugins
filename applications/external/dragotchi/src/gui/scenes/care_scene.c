#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>

#include "care_scene.h"
#include "scenes.h"
#include "../../flipper_structs.h"
#include "../../constants.h"

static void care_cb(void *ctx, uint32_t index) {
    struct ApplicationContext *c = ctx;
    view_dispatcher_send_custom_event(c->view_dispatcher, index);
}

void scene_care_on_enter(void *ctx) {
    struct ApplicationContext *c = ctx;
    Submenu *m = c->care_module;
    submenu_reset(m);
    submenu_set_header(m, "Care");
    submenu_add_item(m, "Feed", 0, care_cb, c);
    submenu_add_item(m, "Play", 1, care_cb, c);
    submenu_add_item(m, "Clean", 2, care_cb, c);
    submenu_add_item(m, "Medicine", 3, care_cb, c);
    submenu_add_item(m, "Scold", 4, care_cb, c);
    submenu_add_item(m, "Lights", 5, care_cb, c);
    view_dispatcher_switch_to_view(c->view_dispatcher, scene_care);
}

bool scene_care_on_event(void *ctx, SceneManagerEvent e) {
    struct ApplicationContext *c = ctx;
    if(e.type != SceneManagerEventTypeCustom) return false;
    struct ThreadsMessage msg = {.type = (enum ThreadsMessageType)(PROCESS_FEED + e.event)};
    furi_message_queue_put(c->threads_message_queue, &msg, FuriWaitForever);
    scene_manager_search_and_switch_to_previous_scene(c->scene_manager, scene_main);
    return true;
}

void scene_care_on_exit(void *ctx) {
    struct ApplicationContext *c = ctx;
    submenu_reset(c->care_module);
}
