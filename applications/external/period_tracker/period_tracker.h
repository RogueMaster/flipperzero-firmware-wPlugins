#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_box.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include <notification/notification_messages.h>
#include "period_tracker_models.h"

#define TAG "PeriodTracker"

// Custom event sent by the date picker scene (after popping itself) to the
// scene that requested a date. Value is far away from menu item indexes.
#define PERIOD_TRACKER_EVENT_DATE_SELECTED 0xDA7Eu

// Forward declarations
typedef struct PeriodTrackerApp PeriodTrackerApp;

// View indexes
typedef enum {
    PeriodTrackerViewSubmenu,
    PeriodTrackerViewTextInput,
    PeriodTrackerViewWidget,
    PeriodTrackerViewVariableItemList,
    PeriodTrackerViewNumberInput,
    PeriodTrackerViewTextBox,
} PeriodTrackerView;

// Scene indexes
typedef enum {
    PeriodTrackerSceneStart,
    PeriodTrackerScenePinEntry,
    PeriodTrackerSceneMainMenu,
    PeriodTrackerSceneAddGirl,
    PeriodTrackerSceneSelectGirl,
    PeriodTrackerSceneGirlMenu,
    PeriodTrackerSceneEditProfile,
    PeriodTrackerSceneLogEvent,
    PeriodTrackerSceneDatePicker,
    PeriodTrackerSceneEventHistory,
    PeriodTrackerSceneCycleStats,
    PeriodTrackerSceneViewPredictions,
    PeriodTrackerSceneDailyDigest,
    PeriodTrackerSceneSettings,
    PeriodTrackerSceneAlertSettings,
    PeriodTrackerScenePinSettings,
    PeriodTrackerScenePinSetup,
    PeriodTrackerSceneCount,
} PeriodTrackerScene;

// Main app structure
struct PeriodTrackerApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;
    VariableItemList* variable_item_list;
    NumberInput* number_input;
    TextBox* text_box;
    FuriString* text_box_store;
    Storage* storage;
    NotificationApp* notifications;

    // App state
    char text_buffer[128];
    uint32_t pin_code;
    uint32_t pin_first_entry;
    uint8_t pin_fails;
    bool pin_locked;
    bool pin_set;
    FuriTimer* pin_error_timer; // Timer for PIN error display

    // Current girl selection
    char current_girl_name[64];

    // Date picker state
    uint16_t temp_year;
    uint8_t temp_month;
    uint8_t temp_day;

    // Log event state
    bool log_event_is_period; // true = period start, false = symptom
    uint8_t log_event_date_type; // 0=today, 1=yesterday, 2=custom

    // Shared prediction buffer to avoid repeated malloc/free cycles
    // Used by get_today_predictions, Daily Digest, and View Predictions
    // Allocate once, reuse throughout app lifetime - eliminates ~6KB heap churn on every launch
    Prediction* prediction_buffer;
    uint16_t prediction_buffer_size; // Currently allocated size (40)
};

// Scene handlers
void period_tracker_scene_start_on_enter(void* context);
bool period_tracker_scene_start_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_start_on_exit(void* context);

void period_tracker_scene_pin_entry_on_enter(void* context);
bool period_tracker_scene_pin_entry_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_pin_entry_on_exit(void* context);

void period_tracker_scene_main_menu_on_enter(void* context);
bool period_tracker_scene_main_menu_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_main_menu_on_exit(void* context);

void period_tracker_scene_add_girl_on_enter(void* context);
bool period_tracker_scene_add_girl_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_add_girl_on_exit(void* context);

void period_tracker_scene_select_girl_on_enter(void* context);
bool period_tracker_scene_select_girl_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_select_girl_on_exit(void* context);

void period_tracker_scene_girl_menu_on_enter(void* context);
bool period_tracker_scene_girl_menu_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_girl_menu_on_exit(void* context);

void period_tracker_scene_edit_profile_on_enter(void* context);
bool period_tracker_scene_edit_profile_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_edit_profile_on_exit(void* context);

void period_tracker_scene_log_event_on_enter(void* context);
bool period_tracker_scene_log_event_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_log_event_on_exit(void* context);

void period_tracker_scene_date_picker_on_enter(void* context);
bool period_tracker_scene_date_picker_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_date_picker_on_exit(void* context);

void period_tracker_scene_event_history_on_enter(void* context);
bool period_tracker_scene_event_history_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_event_history_on_exit(void* context);

void period_tracker_scene_cycle_stats_on_enter(void* context);
bool period_tracker_scene_cycle_stats_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_cycle_stats_on_exit(void* context);

void period_tracker_scene_view_predictions_on_enter(void* context);
bool period_tracker_scene_view_predictions_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_view_predictions_on_exit(void* context);

void period_tracker_scene_daily_digest_on_enter(void* context);
bool period_tracker_scene_daily_digest_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_daily_digest_on_exit(void* context);

void period_tracker_scene_settings_on_enter(void* context);
bool period_tracker_scene_settings_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_settings_on_exit(void* context);

void period_tracker_scene_alert_settings_on_enter(void* context);
bool period_tracker_scene_alert_settings_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_alert_settings_on_exit(void* context);

void period_tracker_scene_pin_settings_on_enter(void* context);
bool period_tracker_scene_pin_settings_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_pin_settings_on_exit(void* context);

void period_tracker_scene_pin_setup_on_enter(void* context);
bool period_tracker_scene_pin_setup_on_event(void* context, SceneManagerEvent event);
void period_tracker_scene_pin_setup_on_exit(void* context);

// Utility functions
bool period_tracker_load_pin(PeriodTrackerApp* app);
bool period_tracker_check_pin_locked(PeriodTrackerApp* app);
void period_tracker_create_data_dir(PeriodTrackerApp* app);
uint8_t period_tracker_load_pin_fails(PeriodTrackerApp* app);
bool period_tracker_save_pin_fails(PeriodTrackerApp* app, uint8_t fails);
void period_tracker_reset_pin_fails(PeriodTrackerApp* app);
