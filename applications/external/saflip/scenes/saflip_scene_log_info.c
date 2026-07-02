#include "../saflip.h"

void saflip_scene_log_info_on_enter(void* context) {
    SaflipApp* app = context;

    LogEntry entry =
        app->log[scene_manager_get_scene_state(app->scene_manager, SaflipSceneLogInfo)];

    widget_reset(app->widget);
    FuriString* temp_str = furi_string_alloc();

    // Show lock ID in bold in top-left corner
    furi_string_printf(temp_str, "%d", entry.lock_id);
    widget_add_string_element(
        app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, furi_string_get_cstr(temp_str));

    // Show date/time on the same line, but not bold and on the right
    if(entry.time_is_set) {
        furi_string_printf(
            temp_str,
            "%04d-%02d-%02d %02d:%02d%s\n",
            entry.time.year,
            entry.time.month,
            entry.time.day,
            entry.time.hour,
            entry.time.minute,
            entry.is_dst ? " [D]" : " [S]");
    } else {
        furi_string_printf(temp_str, "Clock Not Set");
    }
    widget_add_string_element(
        app->widget, 128, 1, AlignRight, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    // Show fields below

    furi_string_printf(temp_str, "Lock Prob: %s\n", entry.lock_problem ? "Yes" : "No");
    widget_add_string_element(
        app->widget, 0, 11, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    furi_string_printf(temp_str, "Low Batt: %s\n", entry.low_battery ? "Yes" : "No");
    widget_add_string_element(
        app->widget, 0, 21, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    furi_string_printf(temp_str, "Let Open: %s\n", entry.let_open ? "Yes" : "No");
    widget_add_string_element(
        app->widget, 0, 31, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    widget_add_line_element(app->widget, 62, 10, 62, 40);

    furi_string_printf(temp_str, "Latched: %s\n", entry.lock_latched ? "Yes" : "No");
    widget_add_string_element(
        app->widget, 66, 11, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    furi_string_printf(temp_str, "Deadbolt: %s\n", entry.deadbolt ? "On" : "Off");
    widget_add_string_element(
        app->widget, 66, 21, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    furi_string_printf(temp_str, "New Key: %s\n", entry.new_key ? "Yes" : "No");
    widget_add_string_element(
        app->widget, 66, 31, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(temp_str));

    widget_add_line_element(app->widget, 0, 41, 128, 41);
    char* description = saflok_log_entry_description(entry);
    if(description == NULL) {
        FuriString* unknown_desc =
            furi_string_alloc_printf("Unknown diagnostic code: %d", entry.diagnostic_code);
        widget_add_text_scroll_element(
            app->widget, 0, 44, 128, 20, furi_string_get_cstr(unknown_desc));
        furi_string_free(unknown_desc);
    } else {
        widget_add_text_scroll_element(app->widget, 0, 44, 128, 20, description);
    }

    furi_string_free(temp_str);
    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewWidget);
}

bool saflip_scene_log_info_on_event(void* context, SceneManagerEvent event) {
    SaflipApp* app = context;
    bool consumed = false;

    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void saflip_scene_log_info_on_exit(void* context) {
    SaflipApp* app = context;
    widget_reset(app->widget);
}
