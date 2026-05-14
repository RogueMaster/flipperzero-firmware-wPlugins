#include "animation_switcher.h"
#include "scenes/fas_scene.h"

#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════
 * ViewDispatcher callbacks
 * ═══════════════════════════════════════════════════════════════════════ */

static bool fas_custom_event_cb(void* context, uint32_t event) {
    FasApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool fas_navigation_event_cb(void* context) {
    FasApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* ═══════════════════════════════════════════════════════════════════════
 * App lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

static FasApp* fas_app_alloc(void) {
    FasApp* app = malloc(sizeof(FasApp));
    furi_assert(app);
    memset(app, 0, sizeof(FasApp));

    /* Open system records */
    app->gui     = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);

    /* Scene manager */
    app->scene_manager = scene_manager_alloc(&fas_scene_handlers, app);

    /* View dispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, fas_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, fas_navigation_event_cb);

    /* ── Allocate views ────────────────────────────────────────────── */

    app->menu = menu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FasViewMenu, menu_get_view(app->menu));

    app->list_view = fas_list_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FasViewList, fas_list_view_get_view(app->list_view));

    app->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FasViewVarList,
        variable_item_list_get_view(app->var_list));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FasViewTextInput, text_input_get_view(app->text_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FasViewWidget, widget_get_view(app->widget));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FasViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    /* Attach to GUI as fullscreen app */
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void fas_app_free(FasApp* app) {
    /* Remove views before freeing them */
    view_dispatcher_remove_view(app->view_dispatcher, FasViewMenu);
    menu_free(app->menu);

    view_dispatcher_remove_view(app->view_dispatcher, FasViewList);
    fas_list_view_free(app->list_view);

    view_dispatcher_remove_view(app->view_dispatcher, FasViewVarList);
    variable_item_list_free(app->var_list);

    view_dispatcher_remove_view(app->view_dispatcher, FasViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, FasViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, FasViewDialogEx);
    dialog_ex_free(app->dialog_ex);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Storage helpers
 * ═══════════════════════════════════════════════════════════════════════ */

void fas_ensure_playlists_dir(FasApp* app) {
    storage_simply_mkdir(app->storage, FAS_PLAYLISTS_PATH);
}

/**
 * Scan /ext/dolphin for subdirectories — each subdirectory is an animation.
 * Resets animation_count and populates the animations[] array with defaults.
 */
bool fas_load_animations(FasApp* app) {
    app->animation_count = 0;

    /* Verify SD card / dolphin folder is accessible */
    if(storage_common_stat(app->storage, FAS_DOLPHIN_PATH, NULL) != FSE_OK) {
        return false;
    }

    File* dir = storage_file_alloc(app->storage);
    if(!storage_dir_open(dir, FAS_DOLPHIN_PATH)) {
        storage_file_free(dir);
        return false;
    }

    FileInfo fi;
    char     name[FAS_ANIM_NAME_LEN];

    while(storage_dir_read(dir, &fi, name, sizeof(name)) &&
        app->animation_count < FAS_MAX_ANIMATIONS) {
        /* Only include subdirectories (not manifest.txt or other files) */
        if(fi.flags & FSF_DIRECTORY) {
            AnimEntry* e = &app->animations[app->animation_count];
            strncpy(e->name, name, FAS_ANIM_NAME_LEN - 1);
            e->name[FAS_ANIM_NAME_LEN - 1] = '\0';
            e->selected      = false;
            e->min_butthurt  = FAS_DEFAULT_MIN_BUTTHURT;
            e->max_butthurt  = FAS_DEFAULT_MAX_BUTTHURT;
            e->min_level     = FAS_DEFAULT_MIN_LEVEL;
            e->max_level     = FAS_DEFAULT_MAX_LEVEL;
            e->weight        = FAS_DEFAULT_WEIGHT;
            app->animation_count++;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    return (app->animation_count > 0);
}

/**
 * Scan the playlists folder for .txt files.
 * Strips the .txt suffix and stores the bare name.
 */
bool fas_load_playlists(FasApp* app) {
    app->playlist_count = 0;
    fas_ensure_playlists_dir(app);

    File* dir = storage_file_alloc(app->storage);
    if(!storage_dir_open(dir, FAS_PLAYLISTS_PATH)) {
        storage_file_free(dir);
        return false;
    }

    FileInfo fi;
    /*
     * Buffer is large enough for the name + ".txt\0".
     * FAS_PLAYLIST_NAME_LEN already includes space for the null terminator,
     * so we add 4 for the extension.
     */
    char name[FAS_PLAYLIST_NAME_LEN + 4];

    while(storage_dir_read(dir, &fi, name, sizeof(name)) &&
        app->playlist_count < FAS_MAX_PLAYLISTS) {
        if(fi.flags & FSF_DIRECTORY) continue;

        int len = (int)strlen(name);
        if(len > 4 && strcmp(name + len - 4, ".txt") == 0) {
            PlaylistEntry* e = &app->playlists[app->playlist_count];
            int  bare_len    = len - 4;
            if(bare_len >= FAS_PLAYLIST_NAME_LEN) bare_len = FAS_PLAYLIST_NAME_LEN - 1;
            memcpy(e->name, name, bare_len);
            e->name[bare_len] = '\0';
            app->playlist_count++;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    return true;
}

/**
 * Write a playlist file containing all currently-selected animations.
 * Format mirrors manifest.txt exactly so it can be directly copied over it.
 */
bool fas_save_playlist(FasApp* app, const char* name) {
    char path[FAS_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s.txt", FAS_PLAYLISTS_PATH, name);

    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return false;
    }

    /* Manifest header */
    const char* header = "Filetype: Flipper Animation Manifest\nVersion: 1\n";
    storage_file_write(f, header, strlen(header));

    /* One block per selected animation */
    for(int i = 0; i < app->animation_count; i++) {
        if(!app->animations[i].selected) continue;
        char buf[256];
        int  len = snprintf(
            buf, sizeof(buf),
            "\nName: %s\n"
            "Min butthurt: %d\n"
            "Max butthurt: %d\n"
            "Min level: %d\n"
            "Max level: %d\n"
            "Weight: %d\n",
            app->animations[i].name,
            app->animations[i].min_butthurt,
            app->animations[i].max_butthurt,
            app->animations[i].min_level,
            app->animations[i].max_level,
            app->animations[i].weight);
        if(len > 0) storage_file_write(f, buf, (uint16_t)len);
    }

    storage_file_close(f);
    storage_file_free(f);
    return true;
}

/**
 * Copy /ext/dolphin/manifest.txt into the playlists folder under the given
 * name.  The manifest format already matches a saved playlist exactly, so no
 * parsing is required.  Returns false if the manifest is missing or the copy
 * fails.
 */
bool fas_import_manifest(FasApp* app, const char* name) {
    if(!fas_manifest_exists(app)) return false;

    char dst[FAS_PATH_LEN];
    snprintf(dst, sizeof(dst), "%s/%s.txt", FAS_PLAYLISTS_PATH, name);

    fas_ensure_playlists_dir(app);

    /* storage_common_copy will not overwrite an existing destination file. */
    storage_simply_remove(app->storage, dst);

    return storage_common_copy(app->storage, FAS_MANIFEST_PATH, dst) == FSE_OK;
}

bool fas_manifest_exists(FasApp* app) {
    return storage_common_stat(app->storage, FAS_MANIFEST_PATH, NULL) == FSE_OK;
}

/**
 * Remove a playlist file from the apps_data folder.
 */
bool fas_delete_playlist(FasApp* app, int index) {
    if(index < 0 || index >= app->playlist_count) return false;
    char path[FAS_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s.txt",
            FAS_PLAYLISTS_PATH, app->playlists[index].name);
    return storage_simply_remove(app->storage, path);
}

/**
 * Copy a saved playlist file over /ext/dolphin/manifest.txt.
 * The Flipper's animation manager reads this file on next boot.
 */
bool fas_apply_playlist(FasApp* app, int index) {
    if(index < 0 || index >= app->playlist_count) return false;

    char src[FAS_PATH_LEN];
    snprintf(src, sizeof(src), "%s/%s.txt",
            FAS_PLAYLISTS_PATH, app->playlists[index].name);

    /* Back up the existing manifest before we replace it.  storage_common_copy
     * refuses to overwrite, so remove any previous .bak first.  Both removes
     * are best-effort: the source files may not exist yet. */
    if(storage_common_stat(app->storage, FAS_MANIFEST_PATH, NULL) == FSE_OK) {
        storage_simply_remove(app->storage, FAS_MANIFEST_BACKUP_PATH);
        storage_common_copy(app->storage, FAS_MANIFEST_PATH, FAS_MANIFEST_BACKUP_PATH);
    }

    storage_simply_remove(app->storage, FAS_MANIFEST_PATH);

    FS_Error err = storage_common_copy(app->storage, src, FAS_MANIFEST_PATH);
    return (err == FSE_OK);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Entry point
 * ═══════════════════════════════════════════════════════════════════════ */

int32_t fas_app_entry(void* p) {
    UNUSED(p);

    FasApp* app = fas_app_alloc();
    fas_ensure_playlists_dir(app);

    /* Start at the main menu */
    scene_manager_next_scene(app->scene_manager, FasSceneMainMenu);

    /* Blocks until the user exits (back from main menu) */
    view_dispatcher_run(app->view_dispatcher);

    fas_app_free(app);
    return 0;
}