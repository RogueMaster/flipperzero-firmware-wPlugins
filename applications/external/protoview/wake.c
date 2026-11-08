/* TPMS wake: a plain, unmodulated 125 kHz carrier from the built-in LF
 * RFID coil, for at most WAKE_MAX_MS per key press. Parked sensors (e.g.
 * Huf/VW 5Q0907275F) answer with a normal 433.92 MHz frame.
 *
 * Threading: every HAL call (start/stop carrier, pin reset) happens on the
 * app's main thread. The one-shot FuriTimer callback runs in the timer
 * service thread and only sets app->wake_expired; the main loop calls
 * wake_poll() on every iteration (at least every WAKE_POLL_SLACK_MS), which
 * performs the stop. This avoids a race between the timer thread and a
 * main-thread wake_stop() (view change / exit) that could otherwise disable
 * the TIM1 bus twice. The timer is armed WAKE_POLL_SLACK_MS early so the
 * carrier is off within WAKE_MAX_MS of the key press.
 *
 * The carrier uses TIM1 (furi_hal_rfid_tim_read_*). The internal CC1101
 * async RX uses TIM2, and nothing in this app touches TIM1, so the CC1101
 * RX keeps running during the burst and can catch the sensor's reply.
 * (The external CC1101 driver's timer is not visible in the SDK headers;
 * it is expected not to be TIM1, which LF RFID owns.) Charging is already
 * suppressed for the whole app lifetime by protoview_app_alloc(). */

#include "app.h"

static void wake_timer_callback(void* ctx) {
    ProtoViewApp* app = ctx;
    app->wake_expired = true; /* Must not draw, block or touch the HAL. */
}

void wake_init(ProtoViewApp* app) {
    app->wake_active = false;
    app->wake_expired = false;
    app->wake_timer = furi_timer_alloc(wake_timer_callback, FuriTimerTypeOnce, app);
}

/* Start one burst. Returns false (and does nothing) if a burst is already
 * running or the timer is missing. */
bool wake_start(ProtoViewApp* app) {
    if(app->wake_active || app->wake_timer == NULL) return false;
    app->wake_expired = false;
    furi_hal_rfid_tim_read_start(WAKE_CARRIER_HZ, WAKE_DUTY_CYCLE);
    app->wake_active = true;
    furi_timer_start(app->wake_timer, furi_ms_to_ticks(WAKE_MAX_MS - WAKE_POLL_SLACK_MS));
    notification_message(app->notification, &sequence_single_vibro);
    return true;
}

/* Idempotent: safe to call when no burst is running. */
void wake_stop(ProtoViewApp* app) {
    if(!app->wake_active) return;
    /* Synchronous: after this the callback cannot fire for this burst. */
    if(app->wake_timer) furi_timer_stop(app->wake_timer);
    furi_hal_rfid_tim_read_stop();
    furi_hal_rfid_pins_reset();
    app->wake_active = false;
    app->wake_expired = false;
}

/* Called from the main loop: performs the stop requested by the timer. */
void wake_poll(ProtoViewApp* app) {
    if(app->wake_active && app->wake_expired) wake_stop(app);
}

/* Exit path: carrier off first, then release the timer. */
void wake_deinit(ProtoViewApp* app) {
    wake_stop(app);
    if(app->wake_timer) {
        furi_timer_free(app->wake_timer);
        app->wake_timer = NULL;
    }
}
