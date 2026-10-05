/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Browsing the reads already on the card.
 *
 * Each read is a directory of its own, so the browser is pointed at the
 * export root and filtered to the text files; picking a report opens it.
 */
#include "../emrtd_i.h"

static void emrtd_scene_saved_show_empty(Emrtd* app) {
    DialogMessage* message = dialog_message_alloc();

    dialog_message_set_header(message, "No saved reads", 64, 8, AlignCenter, AlignTop);
    dialog_message_set_text(
        message,
        "Read a document first.\nEvery read is written to\n" EMRTD_EXPORT_DIR,
        64,
        24,
        AlignCenter,
        AlignTop);
    dialog_message_set_buttons(message, NULL, "Back", NULL);

    dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
}

void emrtd_scene_saved_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    if(!storage_dir_exists(app->storage, EMRTD_EXPORT_DIR)) {
        emrtd_scene_saved_show_empty(app);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".txt", NULL);
    options.base_path = EMRTD_EXPORT_DIR;
    options.skip_assets = true;
    options.hide_dot_files = true;
    options.hide_ext = false;

    /*
     * The browser runs its own loop on this thread and returns when the user
     * has chosen or left, which is why this is done in on_enter and not
     * through a view of ours.
     */
    if(dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &options)) {
        scene_manager_next_scene(app->scene_manager, EmrtdSceneSavedDetail);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool emrtd_scene_saved_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void emrtd_scene_saved_on_exit(void* context) {
    UNUSED(context);
}
