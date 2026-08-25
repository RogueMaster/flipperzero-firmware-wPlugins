#include "../seader_i.h"

#define TAG "Seader:Scene:Reader"

static void seader_scene_reader_render(Seader* seader) {
    Widget* widget = seader_get_widget(seader);
    if(!widget) {
        return;
    }
    widget_reset(widget);

    widget_add_string_element(
        widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "USB SAM Reader");

    char line[48];

    /* Live CCID command indicator: shows where a host connect stops. */
    snprintf(
        line,
        sizeof(line),
        "CCID: %s (%lu)",
        seader_reader_ccid_last_name(),
        (unsigned long)seader_reader_ccid_count());
    widget_add_string_element(widget, 0, 14, AlignLeft, AlignTop, FontSecondary, line);

    /* PowerOn diagnostics: ATR length + last bulk-IN write result. */
    uint16_t atr_len = 0;
    int32_t tx_last = 0;
    seader_reader_ccid_debug(&atr_len, &tx_last);
    snprintf(line, sizeof(line), "atr=%u  tx=%ld", (unsigned)atr_len, (long)tx_last);
    widget_add_string_element(widget, 0, 26, AlignLeft, AlignTop, FontSecondary, line);

    snprintf(
        line, sizeof(line), "Relayed: %lu APDUs", (unsigned long)seader_reader_apdu_count(seader));
    widget_add_string_element(widget, 0, 38, AlignLeft, AlignTop, FontSecondary, line);

    widget_add_string_element(
        widget, 0, 52, AlignLeft, AlignTop, FontSecondary, "USB PC/SC · Back to stop");

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewWidget);
}

static void seader_scene_reader_worker_callback(uint32_t event, void* context) {
    Seader* seader = context;
    view_dispatcher_send_custom_event(seader->view_dispatcher, event);
}

void seader_scene_reader_on_enter(void* context) {
    Seader* seader = context;
    seader_worker_acquire(seader);

    seader_scene_reader_render(seader);

    seader_worker_start(
        seader->worker,
        SeaderWorkerStateReaderEmulation,
        seader->uart,
        seader_scene_reader_worker_callback,
        seader);
}

bool seader_scene_reader_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeaderWorkerEventReaderUpdate) {
            seader_scene_reader_render(seader);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Stop the reader worker and restore USB before leaving.
        seader_worker_release(seader);
        scene_manager_search_and_switch_to_previous_scene(
            seader->scene_manager, SeaderSceneSamPresent);
        consumed = true;
    }

    return consumed;
}

void seader_scene_reader_on_exit(void* context) {
    Seader* seader = context;
    // Idempotent: release again in case exit came from a route other than Back.
    seader_worker_release(seader);
    if(seader->widget) {
        widget_reset(seader->widget);
    }
}
