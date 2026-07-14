#include <stdio.h>

#include "../helpers/ir_helper.h"
#include "../timed_remote.h"
#include "timed_remote_scene.h"

#define IR_DIRECTORY "/ext/infrared"

static FuriString** files;
static size_t nfiles;

static void pick_file(void* context, uint32_t index) {
    TimedRemoteApp* app;

    app = context;
    if(index >= nfiles) return;

    snprintf(
        app->file, sizeof(app->file), "%s/%s", IR_DIRECTORY, furi_string_get_cstr(files[index]));
    view_dispatcher_send_custom_event(app->vd, EVENT_FILE_SELECTED);
}

void scene_browse_enter(void* context) {
    TimedRemoteApp* app;
    size_t i;

    app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select IR File");

    if(ir_files(IR_DIRECTORY, &files, &nfiles)) {
        if(nfiles == 0) {
            submenu_add_item(app->submenu, "(No IR files found)", 0, NULL, NULL);
        } else {
            for(i = 0; i < nfiles; i++) {
                submenu_add_item(app->submenu, furi_string_get_cstr(files[i]), i, pick_file, app);
            }
        }
    } else {
        submenu_add_item(app->submenu, "(Error reading directory)", 0, NULL, NULL);
    }

    view_dispatcher_switch_to_view(app->vd, VIEW_MENU);
}

bool scene_browse_event(void* context, SceneManagerEvent event) {
    TimedRemoteApp* app;

    app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != EVENT_FILE_SELECTED) return false;

    scene_manager_next_scene(app->sm, SCENE_SELECT);
    return true;
}

void scene_browse_exit(void* context) {
    TimedRemoteApp* app;

    app = context;
    submenu_reset(app->submenu);
    if(files == NULL) return;

    ir_files_free(files, nfiles);
    files = NULL;
    nfiles = 0;
}
