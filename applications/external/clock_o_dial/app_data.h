#pragma once

#include "clock.h"
#include "info_screen.h"

typedef struct {
    FuriMutex* mutex;
    TimerConfig cfg;
    ClockFace face; // Runtime-only; the dial is a fixed 12-hour face, calculated once
    AppScreen screen;
    uint8_t info_page; // Which page of the info/options pager is showing, when screen is that
    bool running;
    bool has_been_started;
    uint32_t start_tick;
    uint32_t start_wallclock_secs; // Real time-of-day when the current shift began
    uint32_t elapsed_seconds;
    uint16_t ms_adjust;
    bool finish_sound_played; // Track if finish sound has been played
    uint32_t finish_wallclock_secs; // Real time-of-day when finish was detected; the dial freezes
        // on this instead of live "now" until the user dismisses the finished shift
    uint32_t last_hour_played; // Track last hour that played chime (to avoid repeats)
    uint32_t pause_start_tick; // When the current break began (monotonic), for the fold-in check
    uint32_t pause_start_wallclock; // When the current break began (wall clock), for logging it
    BreakInterval breaks[MAX_BREAKS]; // Logged breaks for the current shift, oldest first
    uint8_t break_count;
    uint32_t sound_state_change_tick; // Drives the sound icon's 5s flash
    uint32_t backlight_state_change_tick; // Drives the backlight icon's 5s flash
    uint32_t eco_state_change_tick; // Drives the eco icon's 5s flash
    uint32_t vibro_state_change_tick; // Drives the vibro icon's 5s flash
    uint32_t break_limit_message_tick; // Drives the "too many breaks" message's 5s display
    uint32_t last_activity_tick; // Last time any button was pressed, for eco mode's idle timer
    uint32_t ok_press_tick; // 0 when OK isn't currently held
    bool ok_hold_triggered; // Reset already fired for the current hold
    uint32_t back_press_tick; // 0 when Back isn't currently held
    bool back_hold_triggered; // Close already fired for the current hold
    uint32_t debug_travel_press_tick; // Start of the current debug time-travel hold, for its ramp
} AppData;

// Seconds since local midnight, per the Flipper's real-time clock.
uint32_t rtc_now_seconds(void);
