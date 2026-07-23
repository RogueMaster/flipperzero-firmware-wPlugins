#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"

void period_tracker_scene_event_history_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    // Reset text box and build scrollable event history
    furi_string_reset(app->text_box_store);
    text_box_reset(app->text_box);

    furi_string_cat_printf(app->text_box_store, "Event History: %s\n\n", app->current_girl_name);

// Use smaller buffer - we only show ~50 events anyway (limited by text size)
// 100 events = 4KB instead of 20KB for MAX_EVENTS
#define HISTORY_BUFFER_SIZE 100
    CycleEvent* events = malloc(sizeof(CycleEvent) * HISTORY_BUFFER_SIZE);
    if(!events) {
        furi_string_cat_printf(app->text_box_store, "Error: Out of memory\n\n");
        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
        text_box_set_font(app->text_box, TextBoxFontText);
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
        return;
    }

    uint16_t event_count = 0;

    // Load only up to HISTORY_BUFFER_SIZE events (reduces memory from 20KB to 4KB).
    // Missing events file (new profile) is a normal empty state, not an error.
    bool loaded = events_load_all(
        app->storage, app->current_girl_name, events, &event_count, HISTORY_BUFFER_SIZE);

    if(!loaded || event_count == 0) {
        furi_string_cat_printf(
            app->text_box_store,
            "No period data logged yet.\n\n"
            "Log your first period start to begin tracking!\n\n");
    } else {
        // Show note if there are more events than we loaded
        if(event_count >= HISTORY_BUFFER_SIZE) {
            furi_string_cat_printf(
                app->text_box_store,
                "Showing %u most recent events\n(file has more)\n\n",
                HISTORY_BUFFER_SIZE);
        } else {
            furi_string_cat_printf(app->text_box_store, "Total events: %u\n\n", event_count);
        }

        // Display events in reverse chronological order (newest first)
        for(int i = event_count - 1; i >= 0; i--) {
            // Check string size to prevent overflow
            if(furi_string_size(app->text_box_store) > 2000) {
                furi_string_cat_printf(app->text_box_store, "\n... (more events)\n");
                break;
            }

            char date_str[16];
            format_date_string(&events[i].date, date_str, sizeof(date_str));

            if(events[i].type == EventTypePeriodStart) {
                furi_string_cat_printf(app->text_box_store, "%s - Period Start\n", date_str);
            } else if(events[i].type == EventTypeSymptom) {
                furi_string_cat_printf(
                    app->text_box_store, "%s - %s\n", date_str, events[i].value);
            }
        }
    }

    // Free events array
    free(events);

    furi_string_cat_printf(
        app->text_box_store,
        "\n=== Manual Editing ===\n"
        "To edit events, use a text editor on your computer.\n\n"
        "File location:\n"
        "/ext/apps_data/period_tracker/%s.csv\n\n"
        "Format:\ndate,event_type,value\n"
        "2025-10-14,period_start,1\n"
        "2025-10-15,symptom,cramps",
        app->current_girl_name);

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    text_box_set_font(app->text_box, TextBoxFontText);
    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
}

bool period_tracker_scene_event_history_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void period_tracker_scene_event_history_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
