#include "minesweeper.h"

#include <string.h> //memset
#include <dolphin/dolphin.h>
#include "scenes/minesweeper_scene.h"
#include "helpers/mine_sweeper_storage.h"
#include "helpers/mine_sweeper_config.h"

static bool minesweeper_custom_event_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    MineSweeperApp* app = (MineSweeperApp*)context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

static bool minesweeper_navigation_event_callback(void* context) {
    furi_assert(context);
    MineSweeperApp* app = (MineSweeperApp*)context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void minesweeper_tick_event_callback(void* context) {
    furi_assert(context);
    MineSweeperApp* app = (MineSweeperApp*)context;
    return scene_manager_handle_tick_event(app->scene_manager);
}

static void app_free(MineSweeperApp* app);

static MineSweeperApp* app_alloc(void) {
    MineSweeperApp* app = (MineSweeperApp*)malloc(sizeof(MineSweeperApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app struct");
        return NULL;
    }
    memset(app, 0, sizeof(MineSweeperApp));

    // NotificationApp Service
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    if(!app->notification) {
        FURI_LOG_E(TAG, "Failed to open notification service");
        goto cleanup;
    }

    // Turn backlight on when app starts
    notification_message(app->notification, &sequence_display_backlight_on);

    // Alloc Scene Manager and set handlers for on_enter, on_event, on_exit
    app->scene_manager = scene_manager_alloc(&minesweeper_scene_handlers, app);
    if(!app->scene_manager) {
        FURI_LOG_E(TAG, "Failed to allocate scene manager");
        goto cleanup;
    }

    // Alloc View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        FURI_LOG_E(TAG, "Failed to allocate view dispatcher");
        goto cleanup;
    }

    //
    // Set View Dispatcher event callback context and callbacks
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, minesweeper_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, minesweeper_navigation_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, minesweeper_tick_event_callback, 100);

    // Set settings state defaults
    app->settings_committed.width_str = furi_string_alloc();
    if(!app->settings_committed.width_str) {
        FURI_LOG_E(TAG, "Failed to allocate width string");
        goto cleanup;
    }
    app->settings_committed.height_str = furi_string_alloc();
    if(!app->settings_committed.height_str) {
        FURI_LOG_E(TAG, "Failed to allocate height string");
        goto cleanup;
    }
    memset(&app->settings_draft, 0, sizeof(app->settings_draft));
    app->is_settings_changed = false;

    // If we cannot read the save file set to default values
    if(!mine_sweeper_read_settings(app)) {
        FURI_LOG_I(TAG, "Cannot read save file, loading defaults");
        app->settings_committed.board_width = 16;
        app->settings_committed.board_height = 7;
        app->settings_committed.difficulty = 0;
        app->settings_committed.ensure_solvable_board = false;
        app->feedback_enabled = 1;
        app->wrap_enabled = 1;

        mine_sweeper_save_settings(app);
    } else {
        FURI_LOG_I(TAG, "Save file loaded sucessfully");
    }

    // Alloc views and add to view dispatcher
    app->start_screen = start_screen_alloc();
    if(!app->start_screen) {
        FURI_LOG_E(TAG, "Failed to allocate start screen");
        goto cleanup;
    }
    view_dispatcher_add_view(
        app->view_dispatcher,
        MineSweeperStartScreenView,
        start_screen_get_view(app->start_screen));

    app->loading = loading_alloc();
    if(!app->loading) {
        FURI_LOG_E(TAG, "Failed to allocate loading view");
        goto cleanup;
    }
    view_dispatcher_add_view(
        app->view_dispatcher, MineSweeperLoadingView, loading_get_view(app->loading));

    app->generating_view = minesweeper_generating_view_alloc();
    if(!app->generating_view) {
        FURI_LOG_E(TAG, "Failed to allocate generating view");
        goto cleanup;
    }
    view_dispatcher_add_view(
        app->view_dispatcher,
        MineSweeperGeneratingScreenView,
        minesweeper_generating_view_get_view(app->generating_view));

    MineSweeperConfig initial_config = {
        .width = app->settings_committed.board_width,
        .height = app->settings_committed.board_height,
        .difficulty = app->settings_committed.difficulty,
        .ensure_solvable = app->settings_committed.ensure_solvable_board,
        .wrap_enabled = app->wrap_enabled,
    };

    if(minesweeper_engine_set_config(&app->game_state, &initial_config) ==
       MineSweeperResultInvalid) {
        FURI_LOG_E(TAG, "Failed to set initial game config");
        goto cleanup;
    }

    app->game_screen = mine_sweeper_game_screen_alloc();
    if(!app->game_screen) {
        FURI_LOG_E(TAG, "Failed to allocate game screen");
        goto cleanup;
    }

    view_dispatcher_add_view(
        app->view_dispatcher,
        MineSweeperGameScreenView,
        mine_sweeper_game_screen_get_view(app->game_screen));

    app->menu_screen = dialog_ex_alloc();
    if(!app->menu_screen) {
        FURI_LOG_E(TAG, "Failed to allocate menu screen");
        goto cleanup;
    }
    view_dispatcher_add_view(
        app->view_dispatcher, MineSweeperMenuView, dialog_ex_get_view(app->menu_screen));

    app->settings_screen = variable_item_list_alloc();
    if(!app->settings_screen) {
        FURI_LOG_E(TAG, "Failed to allocate settings screen");
        goto cleanup;
    }
    view_dispatcher_add_view(
        app->view_dispatcher,
        MineSweeperSettingsView,
        variable_item_list_get_view(app->settings_screen));

    app->confirmation_screen = dialog_ex_alloc();
    if(!app->confirmation_screen) {
        FURI_LOG_E(TAG, "Failed to allocate confirmation screen");
        goto cleanup;
    }
    view_dispatcher_add_view(
        app->view_dispatcher,
        MineSweeperConfirmationView,
        dialog_ex_get_view(app->confirmation_screen));

    app->info_screen = text_box_alloc();
    if(!app->info_screen) {
        FURI_LOG_E(TAG, "Failed to allocate info screen");
        goto cleanup;
    }
    view_dispatcher_add_view(
        app->view_dispatcher, MineSweeperInfoView, text_box_get_view(app->info_screen));

    Gui* gui = furi_record_open(RECORD_GUI);
    if(!gui) {
        FURI_LOG_E(TAG, "Failed to open GUI service");
        goto cleanup;
    }

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    furi_record_close(RECORD_GUI);

    return app;

cleanup:
    app_free(app);
    return NULL;
}

static void app_free(MineSweeperApp* app) {
    if(!app) return;

    if(app->notification) {
        notification_message(app->notification, &sequence_reset_rgb);
    }

    if(app->view_dispatcher) {
        if(app->start_screen) {
            view_dispatcher_remove_view(app->view_dispatcher, MineSweeperStartScreenView);
        }
        if(app->loading) {
            view_dispatcher_remove_view(app->view_dispatcher, MineSweeperLoadingView);
        }
        if(app->generating_view) {
            view_dispatcher_remove_view(app->view_dispatcher, MineSweeperGeneratingScreenView);
        }
        if(app->game_screen) {
            view_dispatcher_remove_view(app->view_dispatcher, MineSweeperGameScreenView);
        }
        if(app->menu_screen) {
            view_dispatcher_remove_view(app->view_dispatcher, MineSweeperMenuView);
        }
        if(app->settings_screen) {
            view_dispatcher_remove_view(app->view_dispatcher, MineSweeperSettingsView);
        }
        if(app->confirmation_screen) {
            view_dispatcher_remove_view(app->view_dispatcher, MineSweeperConfirmationView);
        }
        if(app->info_screen) {
            view_dispatcher_remove_view(app->view_dispatcher, MineSweeperInfoView);
        }
    }

    // Free views
    if(app->loading) {
        loading_free(app->loading);
    }
    if(app->generating_view) {
        minesweeper_generating_view_free(app->generating_view);
    }
    if(app->start_screen) {
        start_screen_free(app->start_screen);
    }
    if(app->game_screen) {
        mine_sweeper_game_screen_free(app->game_screen);
    }
    if(app->menu_screen) {
        dialog_ex_free(app->menu_screen);
    }
    if(app->settings_screen) {
        variable_item_list_free(app->settings_screen);
    }
    if(app->confirmation_screen) {
        dialog_ex_free(app->confirmation_screen);
    }
    if(app->info_screen) {
        text_box_free(app->info_screen);
    }

    if(app->scene_manager) {
        scene_manager_free(app->scene_manager);
    }
    if(app->view_dispatcher) {
        view_dispatcher_free(app->view_dispatcher);
    }

    if(app->settings_committed.width_str) {
        furi_string_free(app->settings_committed.width_str);
    }
    if(app->settings_committed.height_str) {
        furi_string_free(app->settings_committed.height_str);
    }

    if(app->notification) {
        furi_record_close(RECORD_NOTIFICATION);
    }

    // Free app structure
    free(app);
}

int32_t minesweeper_app(void* p) {
    UNUSED(p);

    MineSweeperApp* app = app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Mine Sweeper app allocation failed");
        return -1;
    }

    FURI_LOG_I(TAG, "Mine Sweeper app allocated with size : %d", sizeof(*app));

    dolphin_deed(DolphinDeedPluginGameStart);

    // This will be the initial scene on app startup
    scene_manager_next_scene(app->scene_manager, MineSweeperSceneStartScreen);

    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    FURI_LOG_I(TAG, "Mine Sweeper app freed");

    return 0;
}
