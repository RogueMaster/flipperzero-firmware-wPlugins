#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"

typedef enum {
    GirlMenuLogEvent,
    GirlMenuViewState,
    GirlMenuEventHistory,
    GirlMenuCycleStats,
    GirlMenuViewPredictions,
    GirlMenuEditProfile,
    GirlMenuArchive,
} GirlMenuItem;

// Scene state: which view is currently shown within this scene
#define GIRL_MENU_STATE_MENU     0
#define GIRL_MENU_STATE_SUB_VIEW 1

static void period_tracker_scene_girl_menu_submenu_callback(void* context, uint32_t index) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void period_tracker_scene_girl_menu_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    scene_manager_set_scene_state(
        app->scene_manager, PeriodTrackerSceneGirlMenu, GIRL_MENU_STATE_MENU);

    submenu_reset(app->submenu);

    // Set header with girl's name
    char header[MAX_NAME_LENGTH + 10];
    snprintf(header, sizeof(header), "Profile: %s", app->current_girl_name);
    submenu_set_header(app->submenu, header);

    submenu_add_item(
        app->submenu,
        "Log Event",
        GirlMenuLogEvent,
        period_tracker_scene_girl_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "View Current State",
        GirlMenuViewState,
        period_tracker_scene_girl_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "View Event History",
        GirlMenuEventHistory,
        period_tracker_scene_girl_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "View Cycle Statistics",
        GirlMenuCycleStats,
        period_tracker_scene_girl_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "View Predictions",
        GirlMenuViewPredictions,
        period_tracker_scene_girl_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Edit Profile",
        GirlMenuEditProfile,
        period_tracker_scene_girl_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Archive Profile",
        GirlMenuArchive,
        period_tracker_scene_girl_menu_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
}

bool period_tracker_scene_girl_menu_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        if(scene_manager_get_scene_state(app->scene_manager, PeriodTrackerSceneGirlMenu) ==
           GIRL_MENU_STATE_SUB_VIEW) {
            // Back from "View Current State" returns to this menu instead of
            // popping the scene (which would skip back to the profile list)
            scene_manager_set_scene_state(
                app->scene_manager, PeriodTrackerSceneGirlMenu, GIRL_MENU_STATE_MENU);
            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GirlMenuLogEvent:
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneLogEvent);
            consumed = true;
            break;

        case GirlMenuViewState:
            // Load profile and show current state
            {
                GirlProfile profile;
                if(profile_load(app->storage, app->current_girl_name, &profile)) {
                    // Get last period date if available
                    SimpleDate last_period;
                    bool has_period_data = events_get_last_period_start(
                        app->storage, app->current_girl_name, &last_period);

                    // Reset text box and build scrollable content
                    furi_string_reset(app->text_box_store);
                    text_box_reset(app->text_box);

                    // Build header with girl name
                    furi_string_cat_printf(
                        app->text_box_store, "Current State: %s\n\n", profile.name);

                    // Profile settings
                    furi_string_cat_printf(app->text_box_store, "=== Profile ===\n");
                    furi_string_cat_printf(
                        app->text_box_store, "Cycle Length: %d days\n", profile.cycle_length_days);
                    furi_string_cat_printf(
                        app->text_box_store,
                        "Period Length: %d days\n",
                        profile.period_length_days);
                    furi_string_cat_printf(
                        app->text_box_store,
                        "Contraception: %s\n",
                        profile.uses_contraception ? "Yes" : "No");
                    furi_string_cat_printf(
                        app->text_box_store, "Has PCOS: %s\n", profile.has_pcos ? "Yes" : "No");
                    furi_string_cat_printf(
                        app->text_box_store,
                        "Menopausal: %s\n\n",
                        profile.is_menopausal ? "Yes" : "No");

                    // Notes if present
                    if(strlen(profile.notes) > 0) {
                        furi_string_cat_printf(
                            app->text_box_store, "=== Notes ===\n%s\n\n", profile.notes);
                    }

                    // Period tracking data
                    furi_string_cat_printf(app->text_box_store, "=== Cycle Data ===\n");
                    if(has_period_data) {
                        SimpleDate today;
                        get_today(&today);
                        int32_t days_since = days_between(&last_period, &today);
                        int32_t days_until_next = profile.cycle_length_days - days_since;

                        char last_period_str[16];
                        format_date_string(&last_period, last_period_str, sizeof(last_period_str));

                        furi_string_cat_printf(
                            app->text_box_store, "Last Period:\n  %s\n", last_period_str);
                        furi_string_cat_printf(
                            app->text_box_store, "  (%ld days ago)\n\n", (long)days_since);
                        furi_string_cat_printf(
                            app->text_box_store,
                            "Next Expected:\n  ~%ld days from now\n",
                            (long)days_until_next);
                    } else {
                        furi_string_cat_printf(
                            app->text_box_store,
                            "No period data logged yet.\n\nLog your first period start to begin tracking!");
                    }

                    // Configure text box for scrolling
                    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
                    text_box_set_font(app->text_box, TextBoxFontText);
                    scene_manager_set_scene_state(
                        app->scene_manager, PeriodTrackerSceneGirlMenu, GIRL_MENU_STATE_SUB_VIEW);
                    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
                }
            }
            consumed = true;
            break;

        case GirlMenuEventHistory:
            // Navigate to event history scene
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneEventHistory);
            consumed = true;
            break;

        case GirlMenuCycleStats:
            // Navigate to cycle statistics scene
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneCycleStats);
            consumed = true;
            break;

        case GirlMenuViewPredictions:
            // current_girl_name is already set, show predictions for this girl
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneViewPredictions);
            consumed = true;
            break;

        case GirlMenuEditProfile:
            // Navigate to edit profile scene (state 0 = normal back behavior)
            scene_manager_set_scene_state(app->scene_manager, PeriodTrackerSceneEditProfile, 0);
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneEditProfile);
            consumed = true;
            break;

        case GirlMenuArchive:
            // Archive the profile
            if(profile_archive(app->storage, app->current_girl_name)) {
                widget_reset(app->widget);
                char msg[320];
                snprintf(
                    msg,
                    sizeof(msg),
                    "%s archived.\n\n"
                    "Files in:\n"
                    "/ext/apps_data/"
                    "period_tracker/\n"
                    "%s.archive.*\n\n"
                    "To restore: rename\n"
                    "files, remove\n"
                    ".archive",
                    app->current_girl_name,
                    app->current_girl_name);
                widget_add_string_multiline_element(
                    app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, msg);
                view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
                FURI_LOG_I(TAG, "Profile archived: %s", app->current_girl_name);
            }
            consumed = true;
            break;
        }
    }

    return consumed;
}

void period_tracker_scene_girl_menu_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    submenu_reset(app->submenu);
    widget_reset(app->widget);
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
