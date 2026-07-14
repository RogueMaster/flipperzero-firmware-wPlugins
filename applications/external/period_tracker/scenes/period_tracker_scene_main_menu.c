#include "../period_tracker.h"

typedef enum {
    PeriodTrackerMainMenuDailyDigest,
    PeriodTrackerMainMenuSelectGirl,
    PeriodTrackerMainMenuViewPredictions,
    PeriodTrackerMainMenuSettings,
    PeriodTrackerMainMenuHelp,
    PeriodTrackerMainMenuExit,
} PeriodTrackerMainMenuItem;

// Scene state: which view is currently shown within this scene
#define MAIN_MENU_STATE_MENU 0
#define MAIN_MENU_STATE_HELP 1

static void period_tracker_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void period_tracker_scene_main_menu_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    scene_manager_set_scene_state(
        app->scene_manager, PeriodTrackerSceneMainMenu, MAIN_MENU_STATE_MENU);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Period Tracker");

    submenu_add_item(
        app->submenu,
        "Daily Digest",
        PeriodTrackerMainMenuDailyDigest,
        period_tracker_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Select Profile",
        PeriodTrackerMainMenuSelectGirl,
        period_tracker_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "View All Predictions",
        PeriodTrackerMainMenuViewPredictions,
        period_tracker_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Settings",
        PeriodTrackerMainMenuSettings,
        period_tracker_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Help / About",
        PeriodTrackerMainMenuHelp,
        period_tracker_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Exit",
        PeriodTrackerMainMenuExit,
        period_tracker_scene_main_menu_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
}

bool period_tracker_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        if(scene_manager_get_scene_state(app->scene_manager, PeriodTrackerSceneMainMenu) ==
           MAIN_MENU_STATE_HELP) {
            // Back from Help returns to the menu, not out of the app
            scene_manager_set_scene_state(
                app->scene_manager, PeriodTrackerSceneMainMenu, MAIN_MENU_STATE_MENU);
            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
        } else {
            // Back button from main menu should exit the app, not go back to PIN entry
            FURI_LOG_I(TAG, "Main Menu: back button pressed, exiting app");
            view_dispatcher_stop(app->view_dispatcher);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case PeriodTrackerMainMenuDailyDigest:
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneDailyDigest);
            consumed = true;
            break;
        case PeriodTrackerMainMenuViewPredictions:
            // Clear current_girl_name to show all girls
            app->current_girl_name[0] = '\0';
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneViewPredictions);
            consumed = true;
            break;
        case PeriodTrackerMainMenuSelectGirl:
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneSelectGirl);
            consumed = true;
            break;
        case PeriodTrackerMainMenuSettings:
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneSettings);
            consumed = true;
            break;
        case PeriodTrackerMainMenuHelp:
            // Show scrollable help text
            {
                scene_manager_set_scene_state(
                    app->scene_manager, PeriodTrackerSceneMainMenu, MAIN_MENU_STATE_HELP);
                furi_string_reset(app->text_box_store);
                text_box_reset(app->text_box);

                furi_string_cat_printf(
                    app->text_box_store,
                    "Period Tracker v1.0\n\n"
                    "=== Overview ===\n"
                    "Track menstrual cycles for multiple people with predictions and alerts.\n\n"

                    "=== Features ===\n"
                    "- Multi-user support\n"
                    "- Log period starts (today/yesterday/custom date)\n"
                    "- Log symptoms (with custom dates)\n"
                    "- View event history\n"
                    "- Cycle predictions (60 days ahead)\n"
                    "- PMS, pain, ovulation tracking\n"
                    "- Daily digest with smart alerts\n"
                    "- Configurable alert lead time\n"
                    "- PIN protection\n"
                    "- Profile archiving\n\n"

                    "=== How to Use ===\n"
                    "1. Select a profile or create new\n"
                    "2. Log period start dates\n"
                    "3. Log symptoms as needed\n"
                    "4. View event history for records\n"
                    "5. View predictions for planning\n"
                    "6. Check Daily Digest for alerts\n\n"

                    "=== Profile Settings ===\n"
                    "Edit Profile to adjust:\n"
                    "- Cycle length (20-45 days)\n"
                    "- Period length (2-10 days)\n"
                    "- Health flags (PCOS, etc.)\n"
                    "- Personal notes\n\n"

                    "=== Data Storage ===\n"
                    "All data stored locally on SD card at:\n"
                    "/ext/apps_data/period_tracker/\n\n"
                    "Profiles stored as .profile files\n"
                    "Events stored as .csv files\n\n"
                    "To manually edit events, use a text editor on your computer.\n\n"

                    "=== Predictions ===\n"
                    "Based on cycle history:\n"
                    "- Period Start: When period begins\n"
                    "- PMS: 3 days before period\n"
                    "- Pain: Day before to day after\n"
                    "- Ovulation: Day 12-16 (adaptive)\n\n"

                    "The app learns from cycle history to improve prediction accuracy over time.\n\n"

                    "=== Confidence Indicators ===\n"
                    "\u25cf\u25cf\u25cf High (90%%) - Regular cycles\n"
                    "  Cycles vary by +/-3 days.\n"
                    "  Predictions are very reliable.\n\n"

                    "\u25cf\u25cf\u25cb Moderate (70%%) - Some variation\n"
                    "  Cycles vary by +/-5 days.\n"
                    "  Predictions are fairly reliable.\n\n"

                    "\u25cf\u25cb\u25cb Low (50%%) - Irregular cycles\n"
                    "  Cycles vary by +/-7+ days.\n"
                    "  Predictions have wider ranges.\n\n"

                    "\u25cb\u25cb\u25cb Very Low (30%%) - Chaotic cycles\n"
                    "  Cycles vary by +/-10+ days.\n"
                    "  Predictions are less reliable.\n\n"

                    "=== Date Ranges ===\n"
                    "For irregular cycles, predictions show date ranges:\n"
                    "  (range 2025-10-20 to 2025-10-24)\n"
                    "The period is most likely to start within that 5-day window.\n\n"

                    "=== Cycle Statistics ===\n"
                    "View Cycle Statistics from the profile menu to see:\n"
                    "- Profile's average cycle length\n"
                    "- Cycle regularity classification\n"
                    "- Confidence level explanation\n"
                    "- Tips based on cycle patterns\n\n"

                    "Requires at least 3 logged cycles for statistical analysis.\n\n"

                    "=== Smart Alerts ===\n"
                    "Daily Digest shows upcoming alerts based on Alert Lead Time setting (1-7 days).\n"
                    "Change this in Settings > Alert Lead Time.\n\n"

                    "=== Tips ===\n"
                    "- Log period starts regularly for accurate predictions\n"
                    "- Use custom dates to backfill historical data\n"
                    "- Archive instead of deleting profiles\n"
                    "- Check Daily Digest for upcoming alerts\n\n"

                    "=== Navigation ===\n"
                    "Use Back button to return to previous screen.\n"
                    "Press Back at the Main Menu to exit the app.");

                text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
                text_box_set_font(app->text_box, TextBoxFontText);
                view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
            }
            consumed = true;
            break;
        case PeriodTrackerMainMenuExit:
            // Stop the view dispatcher to exit the app
            view_dispatcher_stop(app->view_dispatcher);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void period_tracker_scene_main_menu_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    submenu_reset(app->submenu);
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
