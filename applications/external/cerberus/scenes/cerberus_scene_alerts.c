#include "../cerberus_i.h"

void cerberus_scene_alerts_on_enter(void* context) {
    Cerberus* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    FuriString* text = furi_string_alloc();

    furi_mutex_acquire(app->log_mutex, FuriWaitForever);
    uint8_t count = app->log_count;
    uint32_t total = app->alert_total;

    if(count == 0) {
        furi_string_set(
            text,
            "No alerts yet.\n\nAll three heads are\ncalm. Open Watch to\nstand guard over your\nairspace.");
    } else {
        furi_string_printf(text, "%lu alert(s) logged\n", (unsigned long)total);
        // newest first
        for(uint8_t i = 0; i < count; i++) {
            uint8_t idx = (uint8_t)((app->log_head + CERBERUS_LOG_MAX - 1 - i) % CERBERUS_LOG_MAX);
            CerberusAlertRecord* r = &app->log[idx];
            furi_string_cat_printf(
                text,
                "%02lu:%02lu %s %u %ddBm\n",
                (unsigned long)(r->timestamp_s / 60),
                (unsigned long)(r->timestamp_s % 60),
                cerberus_threat_name(r->threat),
                (unsigned)r->freq_mhz,
                (int)r->rssi);
        }
    }
    furi_mutex_release(app->log_mutex);

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, CerberusViewWidget);
}

bool cerberus_scene_alerts_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; // back navigation handled by the dispatcher; widget scrolls itself
}

void cerberus_scene_alerts_on_exit(void* context) {
    Cerberus* app = context;
    widget_reset(app->widget);
}
