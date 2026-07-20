#include "../hermes_i.h"

#include <dialogs/dialogs.h>
#include <storage/storage.h>

/* Pick a text file and replay it into the console, a line at a time. A login
 * sequence, a set of U-Boot commands, whatever you would otherwise retype on a
 * keyboard that has one button per character. */

#define SCRIPT_EXTENSION ".txt"

void hermes_scene_script_on_enter(void* context) {
    HermesApp* app = context;

    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    /* The browser needs a base_path that exists. On a Flipper that has never
     * logged a session the app folder is not there yet, so make it first -
     * a no-op when it already exists. */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, SESSION_LOG_DIR);
    furi_record_close(RECORD_STORAGE);

    FuriString* path = furi_string_alloc_set(SESSION_LOG_DIR);

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, SCRIPT_EXTENSION, NULL);
    options.hide_ext = false;
    /* Start where the logs live, since that is the folder the user already has
     * for this app - but the browser is free to go anywhere. */
    options.base_path = SESSION_LOG_DIR;

    const bool picked = dialog_file_browser_show(dialogs, path, path, &options);

    bool loaded = false;
    if(picked) {
        loaded = script_load(app->script, furi_string_get_cstr(path));
    }

    furi_string_free(path);
    furi_record_close(RECORD_DIALOGS);

    if(loaded) {
        char note[64];
        snprintf(
            note,
            sizeof(note),
            "running script %s (%u lines)",
            script_name(app->script),
            script_line_count(app->script));
        session_log_note(app->log, note);

        script_start(app->script);
        console_view_set_script(app->console_view, true, 0, script_line_count(app->script));
    } else if(picked) {
        /* Picked something we could not use: empty, too big, or all comments. */
        hermes_notify_found(app, false);
    }

    /* The browser is modal and has already returned, so there is nothing to
     * show here - go straight back to the console. */
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, HermesSceneConsole);
}

bool hermes_scene_script_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hermes_scene_script_on_exit(void* context) {
    UNUSED(context);
}
