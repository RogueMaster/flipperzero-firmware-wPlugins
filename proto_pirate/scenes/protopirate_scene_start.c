// scenes/protopirate_scene_start.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"

#include "proto_pirate_icons.h"

#define TAG "ProtoPirateSceneStart"

typedef enum {
    SubmenuIndexProtoPirateReceiver,
    SubmenuIndexProtoPirateSaved,
    SubmenuIndexProtoPirateReceiverConfig,
#ifdef ENABLE_SUB_DECODE_SCENE
    SubmenuIndexProtoPirateSubDecode,
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
    SubmenuIndexProtoPirateTimingTuner,
#endif
    SubmenuIndexProtoPirateAbout,
} SubmenuIndex;

// Forward declaration
static void protopirate_scene_start_open_saved_captures(ProtoPirateApp* app);

static void protopirate_scene_start_submenu_callback(void* context, uint32_t index) {
    furi_check(context);
    ProtoPirateApp* app = context;

    // Handle "Saved Captures" directly here, not via custom event
    if(index == SubmenuIndexProtoPirateSaved) {
        protopirate_scene_start_open_saved_captures(app);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, index);
    }
}

static void protopirate_scene_start_open_saved_captures(ProtoPirateApp* app) {
    FURI_LOG_I(TAG, "[1] Opening saved captures browser");
    FURI_LOG_I(TAG, "[1a] PROTOPIRATE_APP_FOLDER = %s", PROTOPIRATE_APP_FOLDER);

    // Check and create folder
    FURI_LOG_D(TAG, "[2] Opening storage");
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage) {
        FURI_LOG_E(TAG, "[2a] Failed to open storage!");
        return;
    }

    FURI_LOG_D(TAG, "[3] Checking folder exists");

    if(!storage_dir_exists(storage, PROTOPIRATE_APP_FOLDER)) {
        FURI_LOG_I(TAG, "[4] Creating folder");
        storage_simply_mkdir(storage, PROTOPIRATE_APP_FOLDER);
    }

#ifndef REMOVE_LOGS
    bool folder_ok = storage_dir_exists(storage, PROTOPIRATE_APP_FOLDER);
    FURI_LOG_D(TAG, "[5] Folder exists: %s", folder_ok ? "yes" : "no");
#endif

    furi_record_close(RECORD_STORAGE);
    FURI_LOG_D(TAG, "[6] Storage closed");

    // Check file_path
    FURI_LOG_D(TAG, "[7] Checking app->file_path");
    if(!app->file_path) {
        FURI_LOG_E(TAG, "[7a] app->file_path is NULL!");
        return;
    }

    // Set starting path
    FURI_LOG_D(TAG, "[8] Setting file_path");
    furi_string_set(app->file_path, PROTOPIRATE_APP_FOLDER);
    FURI_LOG_D(TAG, "[9] file_path set to: %s", furi_string_get_cstr(app->file_path));

    // Configure file browser
    FURI_LOG_D(TAG, "[10] Creating browser_options");
    DialogsFileBrowserOptions browser_options;

    FURI_LOG_D(TAG, "[11] Calling dialog_file_browser_set_basic_options");
    dialog_file_browser_set_basic_options(&browser_options, ".psf", &I_subghz_10px);

    FURI_LOG_D(TAG, "[12] Setting browser_options fields");
    browser_options.base_path = PROTOPIRATE_APP_FOLDER;
    browser_options.skip_assets = true;
    browser_options.hide_dot_files = true;

    FURI_LOG_D(TAG, "[13] Checking app->dialogs");
    FURI_LOG_D(TAG, "[13a] app->dialogs = %p", (void*)app->dialogs);

    if(!app->dialogs) {
        FURI_LOG_E(TAG, "[13b] dialogs is NULL! Trying to open...");
        app->dialogs = furi_record_open(RECORD_DIALOGS);
        if(!app->dialogs) {
            FURI_LOG_E(TAG, "[13c] Still NULL after open attempt!");
            return;
        }
        FURI_LOG_I(TAG, "[13d] dialogs opened successfully");
    }

    FURI_LOG_I(TAG, "[14] === CALLING dialog_file_browser_show ===");
    FURI_LOG_D(TAG, "[14a] dialogs=%p, file_path=%p", (void*)app->dialogs, (void*)app->file_path);

    bool file_selected =
        dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options);

    FURI_LOG_I(TAG, "[15] === RETURNED from dialog_file_browser_show ===");
    FURI_LOG_D(TAG, "[15a] file_selected = %d", file_selected);

    if(file_selected) {
        FURI_LOG_I(TAG, "[16] File selected: %s", furi_string_get_cstr(app->file_path));

        if(app->loaded_file_path) {
            FURI_LOG_D(TAG, "[17] Freeing old loaded_file_path");
            furi_string_free(app->loaded_file_path);
        }

        FURI_LOG_D(TAG, "[18] Allocating new loaded_file_path");
        app->loaded_file_path = furi_string_alloc_set(app->file_path);

        FURI_LOG_D(TAG, "[19] Navigating to SavedInfo scene");
        scene_manager_next_scene(app->scene_manager, ProtoPirateSceneSavedInfo);
    } else {
        FURI_LOG_I(TAG, "[16] File browser cancelled or empty");
    }

    FURI_LOG_I(TAG, "[20] open_saved_captures complete");
}

void protopirate_scene_start_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    protopirate_release_shared_radio_state(app);

    submenu_add_item(
        app->submenu,
        "Receive",
        SubmenuIndexProtoPirateReceiver,
        protopirate_scene_start_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Saved Captures",
        SubmenuIndexProtoPirateSaved,
        protopirate_scene_start_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Configuration",
        SubmenuIndexProtoPirateReceiverConfig,
        protopirate_scene_start_submenu_callback,
        app);
#ifdef ENABLE_SUB_DECODE_SCENE
    submenu_add_item(
        app->submenu,
        "Sub Decode",
        SubmenuIndexProtoPirateSubDecode,
        protopirate_scene_start_submenu_callback,
        app);
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
    submenu_add_item(
        app->submenu,
        "Timing Tuner",
        SubmenuIndexProtoPirateTimingTuner,
        protopirate_scene_start_submenu_callback,
        app);
#endif

    submenu_add_item(
        app->submenu,
        "About",
        SubmenuIndexProtoPirateAbout,
        protopirate_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, ProtoPirateSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewSubmenu);
}

bool protopirate_scene_start_on_event(void* context, SceneManagerEvent event) {
    furi_check(context);
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexProtoPirateAbout) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneAbout);
            consumed = true;
        } else if(event.event == SubmenuIndexProtoPirateReceiver) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiver);
            consumed = true;
        } else if(event.event == SubmenuIndexProtoPirateReceiverConfig) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiverConfig);
            consumed = true;
        }
#ifdef ENABLE_SUB_DECODE_SCENE
        else if(event.event == SubmenuIndexProtoPirateSubDecode) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneSubDecode);
            consumed = true;
        }
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
        else if(event.event == SubmenuIndexProtoPirateTimingTuner) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneTimingTuner);
            consumed = true;
        }
#endif
        scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneStart, event.event);
    }

    return consumed;
}

void protopirate_scene_start_on_exit(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    submenu_reset(app->submenu);
}
