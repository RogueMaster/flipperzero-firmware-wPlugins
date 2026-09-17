/*
 * BEEPBACK - the entry point.
 *
 * Everything the game decides happens in beepback_nav.c and
 * beepback_game.c, which touch no Flipper API at all: time goes in
 * through bb_tick() and the hardware comes out as app->led and
 * app->tone_hz. This file is the only place that talks to the device,
 * which is what lets the rest of it run under test/run_tests.sh.
 */
#include "beepback.h"
#include <furi_hal_random.h>

/* Device-side state, which the game logic has no business knowing. */
static uint16_t bb_tone_playing;
static uint8_t bb_led_shown = BbLedCount; /* nothing applied yet */
static uint8_t bb_led_gen_shown;
static bool bb_speaker_ours;

static void bb_draw_cb(Canvas* canvas, void* ctx) {
    BeepbackApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bb_draw(canvas, app);
    furi_mutex_release(app->mutex);
}

static void bb_input_cb(InputEvent* event, void* ctx) {
    BeepbackApp* app = ctx;
    furi_message_queue_put(app->queue, event, 0);
}

/* ------------------------------------------------------------------ */
/* Hardware, driven from what the logic asked for                      */
/* ------------------------------------------------------------------ */

/* The speaker is taken once and held for as long as the app is in the
 * foreground. Acquiring it per tone meant a handful of acquire/release
 * pairs a second during playback, each of which can fail if anything
 * else wants the speaker, and a failed one is a note that simply does
 * not sound. A game that is asking you to listen cannot drop notes. */
static void bb_audio_apply(BeepbackApp* app) {
    uint16_t want = app->set.volume ? app->tone_hz : 0; /* volume 0 is silent */
    if(want == bb_tone_playing) return;

    if(!bb_speaker_ours) {
        if(want == 0) {
            bb_tone_playing = 0;
            return;
        }
        if(!furi_hal_speaker_acquire(100)) return; /* someone else has it */
        bb_speaker_ours = true;
    }

    if(want == 0) {
        furi_hal_speaker_stop();
    } else {
        /* VOL_GAIN, calibrated for this speaker; the only float in the
           build that gameplay can see, and it stops here */
        float level = (float)bb_vol_gain[app->set.volume % BB_VOL_COUNT] / 100.0f;
        furi_hal_speaker_start((float)want, level);
    }
    bb_tone_playing = want;
}

static void bb_led_apply_changed(BeepbackApp* app) {
    /* colour alone is not enough: two flashes of the same colour back to
       back have to reach the LED as two */
    if(app->led == bb_led_shown && app->led_gen == bb_led_gen_shown) return;
    bb_led_shown = app->led;
    bb_led_gen_shown = app->led_gen;
    bb_led_apply(app, (BbLedColor)app->led);
}

/* the motor, switched only when it actually changes */
static bool bb_buzzing;

static void bb_buzz_apply_changed(BeepbackApp* app) {
    bool want = app->buzz_until != 0;
    if(want == bb_buzzing) return;
    bb_buzzing = want;
    bb_buzz_apply(app, want);
}

static void bb_hardware_off(BeepbackApp* app) {
    app->tone_hz = 0;
    app->buzz_until = 0;
    bb_buzz_apply_changed(app);
    app->led = BbLedOff;
    app->led_gen++;
    bb_audio_apply(app);
    bb_led_apply_changed(app);
    /* and hand the speaker back on the way out */
    if(bb_speaker_ours) {
        furi_hal_speaker_release();
        bb_speaker_ours = false;
    }
}

/* ------------------------------------------------------------------ */

int32_t beepback_app(void* p) {
    UNUSED(p);
    BeepbackApp* app = malloc(sizeof(BeepbackApp));
    if(!app) return 1;
    bb_app_init(app);

    app->queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();

    /* a non-daily run needs a seed the daily must never use */
    app->seed = furi_hal_random_get();

    bb_save_load(app);

    view_port_draw_callback_set(app->view_port, bb_draw_cb, app);
    view_port_input_callback_set(app->view_port, bb_input_cb, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    BbScene was = app->scene;
    uint32_t last = furi_get_tick(); /* the kernel tick is a millisecond */

    while(app->running) {
        InputEvent event;
        FuriStatus status =
            furi_message_queue_get(app->queue, &event, furi_ms_to_ticks(BB_TICK_MS));

        furi_mutex_acquire(app->mutex, FuriWaitForever);

        uint32_t now = furi_get_tick();
        uint32_t dt = now - last;
        last = now;
        if(dt) bb_tick(app, dt);

        if(status == FuriStatusOk) bb_input_event(app, event.key, event.type);

        /* the records are worth keeping the moment a run is over, not
           only when the app closes, so the menu writes them too */
        if(app->scene != was) {
            if(app->scene == BbSceneMenu) bb_save_store(app);
            was = app->scene;
        }

        bb_audio_apply(app);
        bb_led_apply_changed(app);
        bb_buzz_apply_changed(app);

        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
    }

    bb_hardware_off(app);
    bb_save_store(app); /* whatever happened, the records go out */

    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->queue);
    free(app);
    return 0;
}
