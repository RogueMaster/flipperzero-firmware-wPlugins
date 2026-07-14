#include <string.h>

#include "../helpers/ir_helper.h"
#include "../timed_remote.h"
#include "timed_remote_scene.h"

static IrSignalList* signals;

static void pick_signal(void* context, uint32_t index) {
    TimedRemoteApp* app;

    app = context;
    if(signals == NULL || index >= signals->count) return;

    if(app->ir != NULL) infrared_signal_free(app->ir);

    app->ir = infrared_signal_alloc();
    if(app->ir == NULL) return;

    infrared_signal_set_signal(app->ir, signals->items[index].signal);
    strncpy(app->sig, furi_string_get_cstr(signals->items[index].name), sizeof(app->sig) - 1);
    app->sig[sizeof(app->sig) - 1] = '\0';
    view_dispatcher_send_custom_event(app->vd, EVENT_SIGNAL_SELECTED);
}

void scene_select_enter(void* context) {
    TimedRemoteApp* app;
    size_t i;

    app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select Signal");

    signals = ir_list_alloc();
    if(signals == NULL) {
        submenu_add_item(app->submenu, "(Out of memory)", 0, NULL, NULL);
    } else if(ir_load(app->file, signals)) {
        if(signals->count == 0) {
            submenu_add_item(app->submenu, "(No signals in file)", 0, NULL, NULL);
        } else {
            for(i = 0; i < signals->count; i++) {
                submenu_add_item(
                    app->submenu,
                    furi_string_get_cstr(signals->items[i].name),
                    i,
                    pick_signal,
                    app);
            }
        }
    } else {
        submenu_add_item(app->submenu, "(Error reading file)", 0, NULL, NULL);
    }

    view_dispatcher_switch_to_view(app->vd, VIEW_MENU);
}

bool scene_select_event(void* context, SceneManagerEvent event) {
    TimedRemoteApp* app;

    app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != EVENT_SIGNAL_SELECTED) return false;

    scene_manager_next_scene(app->sm, SCENE_CONFIG);
    return true;
}

void scene_select_exit(void* context) {
    TimedRemoteApp* app;

    app = context;
    submenu_reset(app->submenu);
    if(signals == NULL) return;

    ir_list_free(signals);
    signals = NULL;
}
