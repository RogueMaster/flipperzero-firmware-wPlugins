#include "../hotspot_arcade_i.h"
#include "../helpers/ha_flasher.h"

// Runs on the flashing worker thread — only touch volatile progress fields and
// signal the GUI (never block or render here).
static void
    ha_flash_progress_cb(void* ctx, uint8_t idx, uint8_t cnt, uint8_t pct, const char* stage) {
    HotspotArcadeApp* app = ctx;
    app->flash_img = idx;
    app->flash_cnt = cnt;
    app->flash_pct = pct;
    strncpy(app->flash_stage, stage, sizeof(app->flash_stage) - 1);
    app->flash_stage[sizeof(app->flash_stage) - 1] = '\0';
    if(!app->closing)
        view_dispatcher_send_custom_event(app->view_dispatcher, HaEventFlashProgress);
}

// Called (on the worker thread) the moment the board answers in download mode.
static void ha_flash_on_connected(void* ctx) {
    HotspotArcadeApp* app = ctx;
    app->flashing = true; // lock out Back now that writing is imminent
    if(!app->closing) view_dispatcher_send_custom_event(app->view_dispatcher, HaEventFlashStart);
}

static int32_t ha_flash_worker(void* ctx) {
    HotspotArcadeApp* app = ctx;
    char err[80] = "";
    bool ok = ha_flasher_run(
        app->uart,
        furi_string_get_cstr(app->flash_manifest),
        app->flash_auto_boot,
        ha_flash_progress_cb,
        app,
        &app->flash_cancel,
        ha_flash_on_connected,
        err,
        sizeof(err));
    app->flash_ok = ok;
    // flash_finish(reboot=true) asks the ESP to reboot, but in practice the S2 stays
    // in download mode and needs a physical RESET tap, so say so unconditionally.
    // Two lines max: the done screen has a Continue button along the bottom, so a
    // third line would render underneath it.
    strncpy(
        app->flash_msg,
        ok ? "Tap RESET on the board\nto start the new fw." : err, // continues on its own
        sizeof(app->flash_msg) - 1);
    app->flash_msg[sizeof(app->flash_msg) - 1] = '\0';
    app->flashing = false;
    // A cancel (Back while waiting) isn't a result to show — the scene is gone.
    if(!app->closing && !app->flash_cancel)
        view_dispatcher_send_custom_event(app->view_dispatcher, HaEventFlashDone);
    return 0;
}

static void ha_flash_done_button_cb(GuiButtonType result, InputType type, void* context) {
    HotspotArcadeApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter)
        view_dispatcher_send_custom_event(app->view_dispatcher, HaEventFlashContinue);
}

static void ha_flash_render(HotspotArcadeApp* app) {
    widget_reset(app->widget);
    FuriString* t = furi_string_alloc();

    if(app->flash_phase == 0) {
        widget_add_string_element(
            app->widget, 64, 3, AlignCenter, AlignTop, FontPrimary, "Download mode");
        widget_add_line_element(app->widget, 0, 16, 127, 16);
        widget_add_string_multiline_element(
            app->widget,
            64,
            20,
            AlignCenter,
            AlignTop,
            FontSecondary,
            app->flash_auto_boot ?
                "Putting the board into\ndownload mode...\nWaiting for it..." :
                "On the board: hold BOOT,\ntap RESET, release BOOT.\nWaiting for it...");
    } else if(app->flash_phase == 1) {
        widget_add_string_element(
            app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Flashing...");
        widget_add_line_element(app->widget, 0, 20, 127, 20);
        if(app->flash_cnt)
            furi_string_printf(
                t, "Image %u/%u: %s", app->flash_img + 1, app->flash_cnt, app->flash_stage);
        else
            furi_string_set(t, app->flash_stage);
        widget_add_string_element(
            app->widget, 64, 26, AlignCenter, AlignTop, FontSecondary, furi_string_get_cstr(t));
        furi_string_printf(t, "%u%%", app->flash_pct);
        widget_add_string_element(
            app->widget, 64, 39, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(t));
        widget_add_string_element(
            app->widget, 64, 54, AlignCenter, AlignTop, FontSecondary, "Do not disconnect");
    } else {
        widget_add_string_element(
            app->widget,
            64,
            6,
            AlignCenter,
            AlignTop,
            FontPrimary,
            app->flash_ok ? "Flashed!" : "Flash failed");
        widget_add_line_element(app->widget, 0, 20, 127, 20);
        widget_add_string_multiline_element(
            app->widget, 64, 24, AlignCenter, AlignTop, FontSecondary, app->flash_msg);
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Continue", ha_flash_done_button_cb, app);
    }
    furi_string_free(t);
}

void hotspot_arcade_scene_flasher_on_enter(void* context) {
    HotspotArcadeApp* app = context;
    app->flash_phase = 0;
    app->flash_pct = 0;
    app->flash_img = 0;
    app->flash_cnt = 0;
    app->flash_stage[0] = '\0';
    app->flashing = false;
    app->flash_cancel = false;
    app->flash_await_boot = false;
    ha_flash_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HaViewWidget);
    // Auto-detect: the worker polls for download mode and flashes when it appears.
    // Generous stack: the stub-loader connect + flash call chain is deep, and a
    // worker-thread overflow surfaces as an "out of memory" crash.
    app->flash_thread = furi_thread_alloc_ex("HaFlash", 16384, ha_flash_worker, app);
    furi_thread_start(app->flash_thread);
}

bool hotspot_arcade_scene_flasher_on_event(void* context, SceneManagerEvent event) {
    HotspotArcadeApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case HaEventFlashStart: // board detected — switch to the flashing screen
        app->flash_phase = 1;
        ha_flash_render(app);
        return true;
    case HaEventFlashProgress:
        if(app->flash_phase == 1) ha_flash_render(app);
        return true;
    case HaEventFlashDone:
        app->flash_phase = 2;
        // Only after a success: a failed flash can leave the board in download mode,
        // where there's no beacon to wait for, so that path stays manual.
        app->flash_await_boot = app->flash_ok;
        if(app->flash_thread) {
            furi_thread_join(app->flash_thread);
            furi_thread_free(app->flash_thread);
            app->flash_thread = NULL;
        }
        ha_flash_render(app);
        return true;
    case HaEventFlashContinue:
        app->flash_await_boot = false;
        // Return to whoever pushed us: the menu (standalone flash) or the Lobby
        // scene (started with no board — it re-detects the now-flashed board and
        // continues the session start). The ESP just rebooted into the new firmware
        // and its PING beacon takes a moment to resume, but the lobby's re-detect
        // (ha_board_present polls ~2.5 s on re-enter) covers that window.
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case HaEventRefreshView:
        return true; // ignore any stray session RX refresh here
    default:
        return false;
    }
}

void hotspot_arcade_scene_flasher_on_exit(void* context) {
    HotspotArcadeApp* app = context;
    // Stop the poll (if still waiting) and reap the worker. Back is blocked while
    // flashing, so this only runs before connect or after done.
    app->flash_cancel = true;
    app->flash_await_boot = false;
    if(app->flash_thread) {
        furi_thread_join(app->flash_thread);
        furi_thread_free(app->flash_thread);
        app->flash_thread = NULL;
    }
    app->flashing = false;
    widget_reset(app->widget);
}
