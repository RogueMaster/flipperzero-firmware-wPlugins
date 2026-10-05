#include <furi_hal_version.h>
#include <string.h>

#include "debug.h"

#define DEBUG_DEVICE_NAME "Pelavicr"

bool is_debug_device(void) {
    return strcmp(furi_hal_version_get_name_ptr(), DEBUG_DEVICE_NAME) == 0;
}

// Left, Down, Right, Down, Left - entered as plain presses while a shift is active.
static const InputKey debug_combo[] = {
    InputKeyLeft,
    InputKeyDown,
    InputKeyRight,
    InputKeyDown,
    InputKeyLeft,
};
#define DEBUG_COMBO_LENGTH (sizeof(debug_combo) / sizeof(debug_combo[0]))

static InputKey debug_combo_buffer[DEBUG_COMBO_LENGTH];
static bool debug_mode_unlocked = false;

bool is_debug_mode_active(void) {
    return is_debug_device() && debug_mode_unlocked;
}

bool debug_feed_combo_key(InputKey key) {
    if(!is_debug_device()) return false; // the combo means nothing anywhere else

    memmove(
        &debug_combo_buffer[0],
        &debug_combo_buffer[1],
        (DEBUG_COMBO_LENGTH - 1) * sizeof(InputKey));
    debug_combo_buffer[DEBUG_COMBO_LENGTH - 1] = key;

    if(memcmp(debug_combo_buffer, debug_combo, sizeof(debug_combo)) == 0) {
        debug_mode_unlocked = true;
        return true;
    }
    return false;
}

#define DEBUG_TIME_TRAVEL_MINUTE_SECONDS 60
// Never let the overall span from the shift's start to "now" grow past this when fast-forwarding
// - stays comfortably clear of the 24h wrap boundary that wall-clock-of-day arithmetic wraps
// around at.
#define DEBUG_TIME_TRAVEL_MAX_SPAN_SECONDS (20 * 3600)

// Shifts a wall-clock (seconds-of-day) value by delta, wrapping around midnight.
static uint32_t wrap_add_seconds(uint32_t base, int32_t delta) {
    int32_t result = ((int32_t)base + delta) % 86400;
    if(result < 0) result += 86400;
    return (uint32_t)result;
}

// Forward duration in seconds from `from` to `to`, both seconds-of-day, wrapping past midnight.
static uint32_t forward_gap_seconds(uint32_t from, uint32_t to) {
    return to >= from ? to - from : to + 86400 - from;
}

// Shifts a (wallclock, tick) pair together by delta seconds, without ever pushing the tick side
// past `now_tick` - which would make whatever duration it's later subtracted from look negative.
static void shift_tick(uint32_t* tick, uint32_t now_tick, int32_t delta) {
    uint32_t new_tick = *tick + (uint32_t)(delta * 1000);
    *tick = (int32_t)(now_tick - new_tick) >= 0 ? new_tick : now_tick;
}

void debug_time_travel(AppData* app, int32_t minutes) {
    // The whole recorded history - the shift's start and every logged break - slides by the same
    // amount, as one solid block: every segment's own individual duration (each break's length,
    // the work gaps between them) is preserved exactly, only their absolute position in the day
    // changes. What differs between running and paused is only whether that also counts as more
    // (or less) worked time.
    int32_t delta = -minutes * DEBUG_TIME_TRAVEL_MINUTE_SECONDS;
    uint32_t now = rtc_now_seconds();
    uint32_t now_tick = furi_get_tick();

    // The anchor closest to "now" - the current live segment's own start - is what determines
    // how far we can rewind before crossing "now" itself; the shift's overall start is what
    // determines how far we can fast-forward before that overall span nears the wrap boundary.
    uint32_t live_start = app->running ? (app->break_count > 0 ?
                                               app->breaks[app->break_count - 1].end_wallclock_secs :
                                               app->start_wallclock_secs) :
                                          app->pause_start_wallclock;
    if(delta > 0) {
        // Rewinding: don't push the most recent boundary past "now".
        uint32_t live_gap = forward_gap_seconds(live_start, now);
        if((uint32_t)delta > live_gap) delta = (int32_t)live_gap;
    } else if(delta < 0) {
        // Fast-forwarding: don't let the total span since the shift began balloon toward the
        // 24h wrap boundary.
        uint32_t overall_gap = forward_gap_seconds(app->start_wallclock_secs, now);
        uint32_t max_growth = overall_gap < DEBUG_TIME_TRAVEL_MAX_SPAN_SECONDS ?
                                   DEBUG_TIME_TRAVEL_MAX_SPAN_SECONDS - overall_gap :
                                   0;
        if((uint32_t)(-delta) > max_growth) delta = -(int32_t)max_growth;
    }

    app->start_wallclock_secs = wrap_add_seconds(app->start_wallclock_secs, delta);
    for(uint8_t i = 0; i < app->break_count; i++) {
        app->breaks[i].start_wallclock_secs =
            wrap_add_seconds(app->breaks[i].start_wallclock_secs, delta);
        app->breaks[i].end_wallclock_secs =
            wrap_add_seconds(app->breaks[i].end_wallclock_secs, delta);
    }

    if(app->running) {
        // Actively working - this time genuinely counts as worked. elapsed_seconds only ever
        // holds *completed* segments' totals, and the live segment's own duration is entirely
        // captured by start_tick vs the real current tick, so keep that in lockstep with the
        // wall-clock shift above rather than touching elapsed_seconds directly.
        shift_tick(&app->start_tick, now_tick, delta);
    } else {
        // On a break - break time never counts as worked, so elapsed_seconds (and the
        // prediction it drives) stays untouched; only the live break's own bookkeeping moves.
        app->pause_start_wallclock = wrap_add_seconds(app->pause_start_wallclock, delta);
        shift_tick(&app->pause_start_tick, now_tick, delta);
    }
}
