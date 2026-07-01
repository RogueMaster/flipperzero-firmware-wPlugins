#include "kyberwrite_scene.h"
#include <notification/notification_messages.h>

static void build_write_data(KyberApp* app, LFRFIDEM4305* out) {
    for(uint8_t i = 0; i < EM4x05_WORD_COUNT; i++) {
        out->word[i] = KYBER_BASE_DATA[i];
    }
    out->word[6] = KYBER_CRYSTALS[app->crystal_index].addr6;

    if(app->mode == KyberModeFullInit) {
        out->mask = 0xFFFF & ~(1 << 2);
    } else {
        out->mask = (1 << 6);
    }
}

static void update_progress_widget(KyberApp* app) {
    widget_reset(app->writing_widget);

    const char* mode_str = (app->mode == KyberModeFullInit)
        ? "Full init in progress..."
        : "Changing color...";

    widget_add_string_element(
        app->writing_widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "KyberWrite");
    widget_add_string_element(
        app->writing_widget, 64, 28, AlignCenter, AlignCenter, FontSecondary, mode_str);
    widget_add_string_element(
        app->writing_widget, 64, 42, AlignCenter, AlignCenter, FontSecondary,
        KYBER_CRYSTALS[app->crystal_index].name);
    widget_add_string_element(
        app->writing_widget, 64, 56, AlignCenter, AlignCenter, FontSecondary,
        "Hold saber near Flipper");
}

// Timer fires once after a short delay so the progress screen has time
// to render before we block the event loop with the write.
// em4305_write() uses FURI_CRITICAL_ENTER internally (disables interrupts)
// so it MUST run on the GUI/main thread, not in a FuriThread.
static void write_timer_callback(void* context) {
    furi_assert(context);
    KyberApp* app = context;

    LFRFIDEM4305 data;
    build_write_data(app, &data);

    // em4305_write handles its own hardware init/deinit internally via
    // em4305_start() -> furi_hal_rfid_tim_read_start() and em4305_stop().
    // No external HAL setup needed.
    em4305_write(&data);

    app->write_success = true;
    notification_message(app->notifications, &sequence_success);


    static const char* lines[] = { "Read your crystal", "to ensure it wrote" };
    app->hint_title = "Writes are ify";
    app->hint_text_lines = 2;
    app->hint_text = lines;
    app->hint_next_scene = KyberSceneResult;
    if(app->show_hints) {
        view_dispatcher_send_custom_event(app->view_dispatcher, KyberEventShowHint);
    } else {
         view_dispatcher_send_custom_event(app->view_dispatcher, KyberEventWriteComplete);
    }

}

void kyberwrite_scene_writing_on_enter(void* context) {
    furi_assert(context);
    KyberApp* app = context;

    app->write_success = false;

    update_progress_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, KyberViewWriting);

    notification_message(app->notifications, &sequence_blink_start_blue);

    // Short delay so the widget renders before the blocking write starts
    FuriTimer* timer = furi_timer_alloc(write_timer_callback, FuriTimerTypeOnce, app);
    scene_manager_set_scene_state(
        app->scene_manager, KyberSceneWriting, (uint32_t)(uintptr_t)timer);
    furi_timer_start(timer, 200);
}

bool kyberwrite_scene_writing_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    KyberApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == KyberEventWriteComplete || event.event == KyberEventWriteFailed) {
            notification_message(app->notifications, &sequence_blink_stop);
            scene_manager_next_scene(app->scene_manager, KyberSceneResult);
            consumed = true;
        } else if(event.event == KyberEventShowHint) {
            notification_message(app->notifications, &sequence_blink_stop);
            scene_manager_next_scene(app->scene_manager, KyberSceneHint);
            consumed = true;
        }
    }
    return consumed;
}
void kyberwrite_scene_writing_on_exit(void* context) {
    furi_assert(context);
    KyberApp* app = context;

    FuriTimer* timer = (FuriTimer*)(uintptr_t)
        scene_manager_get_scene_state(app->scene_manager, KyberSceneWriting);
    furi_timer_stop(timer);
    furi_timer_free(timer);

    notification_message(app->notifications, &sequence_blink_stop);
    widget_reset(app->writing_widget);
}
