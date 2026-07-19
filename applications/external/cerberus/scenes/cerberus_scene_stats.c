#include "../cerberus_i.h"
#include <input/input.h>

// Scene-local custom event (kept well below CERBERUS_ALERT_EVENT_BASE).
#define CERBERUS_EVENT_STATS_RESET 0x200u

static void cerberus_scene_stats_button_cb(GuiButtonType result, InputType type, void* context) {
    Cerberus* app = context;
    // Don't mutate the widget from inside its own input callback - defer it.
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CERBERUS_EVENT_STATS_RESET);
    }
}

static void cerberus_scene_stats_render(Cerberus* app) {
    Widget* widget = app->widget;
    widget_reset(widget);

    CerberusSnapshot snap;
    cerberus_subghz_get_snapshot(app->worker, &snap);

    furi_mutex_acquire(app->log_mutex, FuriWaitForever);
    uint32_t total = app->alert_total;
    uint32_t jam = app->count_threat[CerberusThreatJam];
    uint32_t flood = app->count_threat[CerberusThreatFlood];
    uint32_t replay = app->count_threat[CerberusThreatReplay];
    uint32_t b0 = app->count_band[0], b1 = app->count_band[1], b2 = app->count_band[2];
    furi_mutex_release(app->log_mutex);

    uint32_t up = (furi_get_tick() - app->start_tick) / 1000;

    FuriString* t = furi_string_alloc();
    furi_string_printf(
        t,
        "Uptime   %02lu:%02lu:%02lu\n"
        "Alerts   %lu total\n"
        "\n"
        "By threat\n"
        "  JAM     %lu\n"
        "  FLOOD   %lu\n"
        "  REPLAY  %lu\n"
        "\n"
        "By band\n"
        "  433 %lu   868 %lu   915 %lu\n"
        "\n"
        "Noise floor (dBm)\n"
        "  433 %d  868 %d  915 %d\n"
        "\n"
        "State    %s\n"
        "SD log   %s",
        (unsigned long)(up / 3600),
        (unsigned long)((up / 60) % 60),
        (unsigned long)(up % 60),
        (unsigned long)total,
        (unsigned long)jam,
        (unsigned long)flood,
        (unsigned long)replay,
        (unsigned long)b0,
        (unsigned long)b1,
        (unsigned long)b2,
        (int)snap.band[0].floor,
        (int)snap.band[1].floor,
        (int)snap.band[2].floor,
        app->armed ? "ARMED" : "SILENT",
        app->config.log_csv ? "ON" : "OFF");

    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "CERBERUS STATS");
    widget_add_text_scroll_element(widget, 0, 14, 128, 38, furi_string_get_cstr(t));
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Reset", cerberus_scene_stats_button_cb, app);
    furi_string_free(t);
}

void cerberus_scene_stats_on_enter(void* context) {
    Cerberus* app = context;
    cerberus_scene_stats_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CerberusViewWidget);
}

bool cerberus_scene_stats_on_event(void* context, SceneManagerEvent event) {
    Cerberus* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == CERBERUS_EVENT_STATS_RESET) {
        cerberus_reset_stats(app);
        notification_message(app->notifications, &sequence_blink_blue_100);
        cerberus_scene_stats_render(app); // safe here: not inside widget input
        return true;
    }
    return false;
}

void cerberus_scene_stats_on_exit(void* context) {
    Cerberus* app = context;
    widget_reset(app->widget);
}
