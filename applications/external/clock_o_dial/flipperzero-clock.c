#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>

#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "clock.h"
#include "app_data.h"
#include "debug.h"
#include "info_screen.h"

// Seconds since local midnight, per the Flipper's real-time clock.
uint32_t rtc_now_seconds(void) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    return (uint32_t)dt.hour * 3600 + (uint32_t)dt.minute * 60 + dt.second;
}

#define CFG_FILENAME APP_DATA_PATH("timer.cfg")

// "loader close" (the CLI command, and what `ufbt launch` sends before relaunching) asks the app
// to exit via a FuriSignalExit signal, not a real button press - with no signal callback
// registered, the loader gets no acknowledgement and tells the user to close it manually. Answer
// by injecting the same synthetic software Back press the input loop already treats as an
// immediate-close request (see the INPUT_SEQUENCE_SOURCE_SOFTWARE check below), so the loader
// sees the app actually exit.
static bool app_signal_callback(uint32_t signal, void* arg, void* context) {
    UNUSED(arg);
    if(signal != FuriSignalExit) return false;
    FuriMessageQueue* event_queue = context;
    InputEvent event = {
        .sequence_source = INPUT_SEQUENCE_SOURCE_SOFTWARE,
        .key = InputKeyBack,
        .type = InputTypePress,
    };
    furi_message_queue_put(event_queue, &event, 100);
    return true;
}

// Adaptive frame rates for battery optimization
#define FRAME_MS_RUNNING       1000 // 1 FPS when timer is running
#define MUTEX_TIMEOUT_IDLE     100 // Short timeout when idle (draw callback won't be called often)
#define FINISH_CHECK_INTERVAL  1000 // Check finish every second when close
#define FINISH_CHECK_THRESHOLD 60 // Only check frequently within last 60 seconds
#define HOLD_PROGRESS_FRAME_MS (1000 / 12) // ~12fps while a hold's progress bar is filling

// Forward declarations
static void
    play_rick_roll_melody(NotificationApp* notification, bool sound_enabled, bool vibro_enabled);
static void play_hour_chime(NotificationApp* notification, bool sound_enabled, bool vibro_enabled);

static void set_backlight(NotificationApp* notification, bool on) {
    // Only toggle the always-on lock, never force it off directly - an explicit "off" fights
    // the input service's own wake-on-press behavior and flickers. Releasing the lock instead
    // just lets the backlight follow the user's own normal auto-dim settings.
    if(on) {
        notification_message_block(notification, &sequence_display_backlight_enforce_on);
    } else {
        notification_message_block(notification, &sequence_display_backlight_enforce_auto);
    }
}

static void cfg_save_internal(File* file, TimerConfig* cfg);

// Nothing is running yet in Set mode, so closing from there needs only half the hold.
static uint32_t back_hold_required_ms(const AppData* app) {
    return app->has_been_started ? HOLD_CONFIRM_MS : HOLD_CONFIRM_MS / 2;
}

// Sound, eco, backlight, and vibro share one status-icon slot; whichever changed most recently
// wins it. Used both to decide what to render and, at input time, whether a press should reveal
// or confirm - an option only confirms when its own icon is the one actually on screen right
// now. Sound-off counts as always showing while muted (it never expires on its own), but
// anything else that changed more recently still wins the slot over it until its own flash
// expires. Mute can only be toggled while working/on break, so its icon (including the
// persistent muted state) is hidden in Set mode too.
static TopRightIconSlot current_top_right_icon_slot(const AppData* app, uint32_t now) {
    // Elapsed-since-change, not the raw ticks themselves - the ticks start out at a huge
    // underflowed sentinel (see clock_main's init) so a fresh boot reads as "long ago" once
    // subtracted from `now`. Comparing the raw tick values against each other instead would see
    // that sentinel as enormous rather than old, letting a never-touched icon's tick beat a real
    // one that just changed.
    uint32_t sound_elapsed = now - app->sound_state_change_tick;
    uint32_t eco_elapsed = now - app->eco_state_change_tick;
    uint32_t backlight_elapsed = now - app->backlight_state_change_tick;
    uint32_t vibro_elapsed = now - app->vibro_state_change_tick;

    bool sound_showing = app->has_been_started &&
                          (!app->cfg.sound_enabled || sound_elapsed < ICON_FLASH_MS);
    bool eco_showing = eco_elapsed < ICON_FLASH_MS;
    bool backlight_showing = backlight_elapsed < ICON_FLASH_MS;
    bool vibro_showing = vibro_elapsed < ICON_FLASH_MS;

    TopRightIconSlot winner = TopRightIconNone;
    uint32_t winner_elapsed = 0;
    bool have_winner = false;

    if(sound_showing) {
        winner = TopRightIconSound;
        // Plain elapsed time, same as the other three: a mute that's actually been sitting
        // untouched keeps growing and naturally loses ties to a genuinely recent press of
        // something else, while a fresh press of Up (reveal or confirm) resets this to ~0 and
        // fairly competes for/wins the slot just like any other icon's own fresh press would.
        winner_elapsed = sound_elapsed;
        have_winner = true;
    }
    if(eco_showing && (!have_winner || eco_elapsed <= winner_elapsed)) {
        winner = TopRightIconEco;
        winner_elapsed = eco_elapsed;
        have_winner = true;
    }
    if(backlight_showing && (!have_winner || backlight_elapsed <= winner_elapsed)) {
        winner = TopRightIconBacklight;
        winner_elapsed = backlight_elapsed;
        have_winner = true;
    }
    if(vibro_showing && (!have_winner || vibro_elapsed <= winner_elapsed)) {
        winner = TopRightIconVibro;
        have_winner = true;
    }
    return have_winner ? winner : TopRightIconNone;
}

// Resets a shift back to a fresh, unconfigured Set-mode state - used by the OK hold-to-reset,
// and by tapping OK or Back while sitting at a finished shift.
static void reset_to_shift_mode(AppData* app) {
    app->has_been_started = false;
    app->running = false;
    app->elapsed_seconds = 0;
    app->ms_adjust = 0;
    app->start_tick = 0;
    app->start_wallclock_secs = 0;
    app->pause_start_tick = 0;
    app->pause_start_wallclock = 0;
    app->break_count = 0;
    app->finish_sound_played = false;
    app->last_hour_played = 0;
    // Otherwise a message shown just before resetting could keep drawing over the Set screen
    // for the rest of its 5s window.
    app->break_limit_message_tick = (uint32_t)(0 - BREAK_LIMIT_MESSAGE_MS);
}

// How many minutes one debug time-travel step covers, given how long the key's been held -
// starts at a single minute for precise single-tap control, then ramps up through progressively
// bigger intervals so holding it can fly through a whole shift in a couple of seconds.
static int32_t debug_travel_step_minutes(uint32_t held_ms) {
    static const uint32_t thresholds_ms[] = {750, 1500, 2250, 3000, 3750, 4500, 5250, 6000};
    static const int32_t steps_minutes[] = {1, 5, 15, 30, 60, 120, 240, 480, 720};
    for(size_t i = 0; i < COUNT_OF(thresholds_ms); i++) {
        if(held_ms < thresholds_ms[i]) return steps_minutes[i];
    }
    return steps_minutes[COUNT_OF(steps_minutes) - 1]; // past every threshold - cap at 12h/step
}

static void adjust_shift_duration(AppData* app, File* file, bool increase) {
    if(furi_mutex_acquire(app->mutex, 100) != FuriStatusOk) return;
    if(!app->has_been_started) {
        if(increase) {
            modify_timer_up(&app->cfg);
        } else {
            modify_timer_down(&app->cfg);
        }
        cfg_save_internal(file, &app->cfg);
    }
    furi_mutex_release(app->mutex);
}

// Fills *label/*fraction and returns true if a qualifying reset/close hold is past HOLD_SHOW_MS.
static bool
    get_hold_overlay(AppData* app, uint32_t now_tick, const char** label, float* fraction) {
    uint32_t hold_ms;
    uint32_t required_ms;
    if(app->ok_press_tick != 0 && !app->ok_hold_triggered) {
        hold_ms = now_tick - app->ok_press_tick;
        if(app->has_been_started) {
            required_ms = HOLD_CONFIRM_MS;
            *label = "RESETTING";
        } else {
            required_ms = INFO_HOLD_MS;
            *label = "INFO";
        }
    } else if(app->back_press_tick != 0 && !app->back_hold_triggered) {
        hold_ms = now_tick - app->back_press_tick;
        required_ms = back_hold_required_ms(app);
        *label = "CLOSING";
    } else {
        return false;
    }
    if(hold_ms < HOLD_SHOW_MS) return false;
    *fraction = (float)hold_ms / (float)required_ms;
    return true;
}

static void app_draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    AppData* app = (AppData*)ctx;

    // Use timeout based on timer state for battery optimization
    // When not running, use short timeout since draw callback won't be called often anyway
    uint32_t timeout_ms = app->running ? FRAME_MS_RUNNING : MUTEX_TIMEOUT_IDLE;

    if(furi_mutex_acquire(app->mutex, timeout_ms) != FuriStatusOk) return;

    if(app->screen == ScreenInfo) {
        uint8_t page = app->info_page;
        furi_mutex_release(app->mutex);
        draw_info_screen(canvas, page);
        return;
    }

    uint32_t current_tick = furi_get_tick();
    uint32_t elapsed_seconds = app->elapsed_seconds;
    uint16_t ms = 0;

    if(app->running) {
        uint32_t elapsed_ms = current_tick - app->start_tick;
        elapsed_seconds = app->elapsed_seconds + (elapsed_ms / 1000);
        ms = elapsed_ms % 1000;
    } else {
        // When paused, use stored milliseconds
        ms = app->ms_adjust;
    }

    // Once progress reaches 1.0, clamp the *locally displayed* elapsed time so the screen shows
    // the finished state instantly instead of waiting for the main loop's next throttled check.
    // This must never mutate app->elapsed_seconds/finish_sound_played/running itself - the main
    // loop is the sole authority for that transition and for playing the finish melody; if this
    // draw callback also flipped finish_sound_played, whichever of the two won the race would
    // silently swallow it, and only the main loop's path actually plays the melody.
    if(app->running && !app->finish_sound_played) {
        uint32_t timer_duration_seconds = app->cfg.timer_duration_hours * 3600;
        uint32_t remaining = timer_duration_seconds > elapsed_seconds ?
                                 timer_duration_seconds - elapsed_seconds :
                                 0;

        // Only check finish if within threshold (saves CPU)
        if(remaining <= FINISH_CHECK_THRESHOLD) {
            float total_ms = elapsed_seconds * 1000.0f + ms;
            float progress = total_ms / (timer_duration_seconds * 1000.0f);

            if(progress >= 1.0f && app->has_been_started) {
                elapsed_seconds = timer_duration_seconds;
                ms = 0;
            }
        }
    }

    uint32_t timer_duration_ms_total = (uint32_t)app->cfg.timer_duration_hours * 3600 * 1000;
    bool is_break = app->has_been_started && !app->running &&
                    (elapsed_seconds * 1000 + ms) < timer_duration_ms_total;

    BreakLog break_log = {
        .items = app->breaks,
        .count = app->break_count,
        .live_active = is_break,
        .live_start_wallclock_secs = app->pause_start_wallclock,
    };

    bool eco_frozen = app->cfg.eco_mode_enabled &&
                      (current_tick - app->last_activity_tick) >= ECO_IDLE_MS;
    uint32_t real_now_wallclock_secs = rtc_now_seconds();

    // Blips for the first second after the label's own minute last changed, so the animation
    // never visibly desyncs from it. Working has a live shift clock to sync to; everything else
    // (Set mode's static duration, a break's frozen countdown, a finished shift's frozen dial)
    // doesn't, so it falls back to a real-time once-a-minute "still alive" tick instead.
    // ms_into_minute == 0 means sitting exactly on the boundary but not yet past it (the label
    // itself hasn't flipped there either - see ms_to_next_minute), so it's excluded here too.
    uint32_t shift_ms_into_minute = (elapsed_seconds * 1000 + ms) % 60000;
    bool blip_active = app->running ? (shift_ms_into_minute >= 1 && shift_ms_into_minute < 1000) :
                                       (real_now_wallclock_secs % 60 == 0);

    UiOverlay ui = {
        .sound_enabled = app->cfg.sound_enabled,
        .backlight_on = app->cfg.backlight_on,
        .eco_mode_enabled = app->cfg.eco_mode_enabled,
        .vibro_enabled = app->cfg.vibro_enabled,
        .long_time_format = app->cfg.long_time_format,
        .top_right_icon_slot = current_top_right_icon_slot(app, current_tick),
        .animations_frozen = eco_frozen,
        .eco_blip_active = eco_frozen && blip_active,
        .hold_active = false,
        .hold_fraction = 0.0f,
        .hold_label = NULL,
        .break_limit_message = (current_tick - app->break_limit_message_tick) <
                                BREAK_LIMIT_MESSAGE_MS,
    };
    ui.hold_active = get_hold_overlay(app, current_tick, &ui.hold_label, &ui.hold_fraction);

    // Sitting at a finished shift: freeze the hand (and everything else "now"-derived) on the
    // moment finish was detected, rather than letting it keep drifting with real time while the
    // user looks the completed shift over. Any button dismisses it back to Set mode. The mascot's
    // sleep animation still needs real time, though, so it's handed the live clock separately.
    uint32_t now_wallclock_secs =
        app->finish_sound_played ? app->finish_wallclock_secs : real_now_wallclock_secs;

    draw_timer(
        canvas,
        &app->face,
        app->cfg.timer_duration_hours,
        elapsed_seconds,
        ms,
        app->running,
        app->has_been_started,
        now_wallclock_secs,
        real_now_wallclock_secs,
        app->start_wallclock_secs,
        &break_log,
        &ui);
    furi_mutex_release(app->mutex);
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static bool cfg_load(File* file, AppData* app) {
    // Always start from known-good defaults (8-hour shift, every option at its documented
    // default) before touching the file, so a missing file, a truncated/corrupted one, or one
    // whose version byte happens to land on or past CONFIG_VERSION by chance can never leave any
    // field holding a garbage value - only a fully-read config overrides these below, and even
    // then only with values that pass the range check further down.
    init_timer_config(&app->cfg);

    size_t readed = 0;
    TimerConfig loaded;
    if(storage_file_open(file, CFG_FILENAME, FSAM_READ, FSOM_OPEN_EXISTING))
        readed = storage_file_read(file, &loaded, sizeof(TimerConfig));
    storage_file_close(file);

    if(readed != sizeof(TimerConfig)) return false;
    app->cfg = loaded;

    // Handle config version migration
    if(app->cfg.version < CONFIG_VERSION) {
        // Migrate from old versions
        if(app->cfg.version < 4) {
            // Old versions had digits_mod, migrate to timer_duration_hours
            app->cfg.timer_duration_hours = DEFAULT_TIMER_HOURS;
        }
        // Version 4 had 'width' field which was removed in version 5
        // Version 5 had 'ofs_x' field which was removed in version 6 (always OFS_LEFT_X)
        // Version 6 had 'face' field which was removed in version 7 (recalculated from timer_duration_hours)
        // Version 7 didn't have 'fill_enabled' field, added in version 8
        // Version 8's 'fill_enabled' was replaced by 'sound_enabled' in version 9 (segments
        // are always shown now; sound can be muted instead)
        if(app->cfg.version < 9) {
            app->cfg.sound_enabled = true; // Default to enabled for old configs
        }
        // Version 10 added 'backlight_on' (previously runtime-only, not persisted)
        if(app->cfg.version < 10) {
            app->cfg.backlight_on = true; // Default to enabled for old configs
        }
        // Version 11 added 'eco_mode_enabled'
        if(app->cfg.version < 11) {
            app->cfg.eco_mode_enabled = true; // Default to enabled for old configs
        }
        // Version 12 added 'vibro_enabled'
        if(app->cfg.version < 12) {
            app->cfg.vibro_enabled = true; // Default to enabled for old configs
        }
        // Version 13 added 'long_time_format'
        if(app->cfg.version < 13) {
            app->cfg.long_time_format = false; // Default to short format for old configs
        }
        // Old configs will fail to load due to size mismatch and be recreated
        app->cfg.version = CONFIG_VERSION;
    }
    // Always validate, regardless of whether migration ran above - a corrupted-but-right-sized
    // file could otherwise carry an out-of-range value straight through untouched if its version
    // byte happened to already read as CONFIG_VERSION or higher.
    if(app->cfg.timer_duration_hours < 1 || app->cfg.timer_duration_hours > MAX_TIMER_HOURS) {
        app->cfg.timer_duration_hours = DEFAULT_TIMER_HOURS;
    }
    return true;
}

static void cfg_save_internal(File* file, TimerConfig* cfg) {
    // Face is not saved - it's recalculated on load
    if(storage_file_open(file, CFG_FILENAME, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(file, cfg, sizeof(TimerConfig));
    storage_file_close(file);
}

static void cfg_save(File* file, AppData* app) {
    if(furi_mutex_acquire(app->mutex, FuriWaitForever) != FuriStatusOk) return;
    cfg_save_internal(file, &app->cfg);
    furi_mutex_release(app->mutex);
}

// Rick Roll melody - "Never gonna give you up" opening phrase
// Exact notes and timing to match the iconic melody
// Based on tracker.c note_to_freq() logic and actual song timing
// The LED and vibro motor follow the same note rhythm independently of sound_enabled, so
// shift-end is still noticeable with sound, light, and/or vibro muted in any combination.
static void
    play_rick_roll_melody(NotificationApp* notification, bool sound_enabled, bool vibro_enabled) {
// Frequency constants for clarity
#define NOTE_A4  440.0f
#define NOTE_B4  493.88f
#define NOTE_D5  587.33f
#define NOTE_E5  659.25f
#define NOTE_FS5 739.99f

    float notes[] = {
        NOTE_A4, // Ne-
        NOTE_B4, // -ver
        NOTE_D5, // gon-
        NOTE_B4, // -na
        NOTE_FS5, // give
        NOTE_FS5, // you
        NOTE_E5 // up
    };

    uint16_t durations[] = {130, 130, 130, 130, 260, 260, 450};

    uint16_t pauses[] = {30, 30, 30, 30, 40, 40, 0};

    // Acquire speaker (similar to tracker_speaker_init)
    bool speaker_ready = sound_enabled && furi_hal_speaker_acquire(1000);

    // Play each note with exact timing, blinking blue and vibrating in step whether or not
    // sound plays
    for(int i = 0; i < 7; i++) {
        // Set/reset (not force-off) through the notification service, so the charging LED
        // indicator gets its color back afterward instead of being left dark while plugged in.
        notification_message_block(notification, &sequence_set_only_blue_255);
        if(vibro_enabled) furi_hal_vibro_on(true);
        if(speaker_ready) furi_hal_speaker_start(notes[i], 0.5f); // frequency, volume (0.5 = 50%)
        furi_delay_ms(durations[i]);
        if(speaker_ready) furi_hal_speaker_stop();
        notification_message_block(notification, &sequence_reset_blue);
        if(vibro_enabled) furi_hal_vibro_on(false);
        if(i < 6) {
            furi_delay_ms(pauses[i]); // Pause between notes
        }
    }

    if(speaker_ready) furi_hal_speaker_release();
}

// Play satisfying "tu-tum" chime for hourly milestones
// Two low notes: ascending for positive feel
// The LED and vibro motor follow the same "tu-tum" rhythm independently of sound_enabled, so
// hour milestones are still noticeable with sound, light, and/or vibro muted in any combination.
static void play_hour_chime(NotificationApp* notification, bool sound_enabled, bool vibro_enabled) {
    // Low satisfying notes - ascending "tu-tum" for positive feel
    // First note: A2 (110.00 Hz) - "tu"
    // Second note: C3 (130.81 Hz) - "tum" (higher, more positive)
    float note1 = 110.00f; // A2
    float note2 = 130.81f; // C3

    // Acquire speaker
    bool speaker_ready = sound_enabled && furi_hal_speaker_acquire(1000);

    // Play first note "tu" - set/reset (not force-off) through the notification service, so the
    // charging LED indicator gets its color back afterward instead of being left dark while
    // plugged in.
    notification_message_block(notification, &sequence_set_only_blue_255);
    if(vibro_enabled) furi_hal_vibro_on(true);
    if(speaker_ready) furi_hal_speaker_start(note1, 0.4f); // Lower volume for subtlety
    furi_delay_ms(150);
    if(speaker_ready) furi_hal_speaker_stop();
    notification_message_block(notification, &sequence_reset_blue);
    if(vibro_enabled) furi_hal_vibro_on(false);

    // Short pause between notes
    furi_delay_ms(50);

    // Play second note "tum" (ascending)
    notification_message_block(notification, &sequence_set_only_blue_255);
    if(vibro_enabled) furi_hal_vibro_on(true);
    if(speaker_ready) furi_hal_speaker_start(note2, 0.4f);
    furi_delay_ms(200); // Slightly longer for the "tum"
    if(speaker_ready) furi_hal_speaker_stop();
    notification_message_block(notification, &sequence_reset_blue);
    if(vibro_enabled) furi_hal_vibro_on(false);

    if(speaker_ready) furi_hal_speaker_release();
}

int32_t clock_main(void* p) {
    UNUSED(p);

    AppData* app = malloc(sizeof(AppData));
    furi_assert(app);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->screen = ScreenClock;
    app->info_page = 0;
    app->running = false;
    app->has_been_started = false;
    app->elapsed_seconds = 0;
    app->start_tick = 0;
    app->start_wallclock_secs = 0;
    app->ms_adjust = 0;
    app->finish_sound_played = false;
    app->last_hour_played = 0;
    app->pause_start_tick = 0;
    app->pause_start_wallclock = 0;
    app->break_count = 0;
    // Far enough in the past that neither flash icon shows before an explicit toggle
    app->sound_state_change_tick = (uint32_t)(0 - ICON_FLASH_MS);
    app->backlight_state_change_tick = (uint32_t)(0 - ICON_FLASH_MS);
    app->eco_state_change_tick = (uint32_t)(0 - ICON_FLASH_MS);
    app->vibro_state_change_tick = (uint32_t)(0 - ICON_FLASH_MS);
    app->break_limit_message_tick = (uint32_t)(0 - BREAK_LIMIT_MESSAGE_MS);
    // The user just interacted with the device to launch the app, so start the idle clock now
    app->last_activity_tick = furi_get_tick();
    app->ok_press_tick = 0;
    app->ok_hold_triggered = false;
    app->back_press_tick = 0;
    app->back_hold_triggered = false;
    app->debug_travel_press_tick = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!cfg_load(file, app)) {
        // cfg_load already left app->cfg at its defaults - just persist them for next boot.
        cfg_save(file, app);
    }
    // The dial is a fixed 12-hour clock face, independent of shift duration
    calc_clock_face(&app->face);

    ViewPort* view_port = view_port_alloc();
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    furi_thread_set_signal_callback(furi_thread_get_current(), app_signal_callback, event_queue);

    view_port_draw_callback_set(view_port, app_draw_callback, app);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    Gui* gui = (Gui*)furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    set_backlight(notification, app->cfg.backlight_on);

    InputEvent event;
    bool terminate = false;
    uint32_t last_finish_check = 0;
    uint32_t last_hour_check = 0;

    while(!terminate) {
        // Adaptive finish check - only check frequently when close to finish
        uint32_t current_tick = furi_get_tick();
        bool should_check_finish = false;
        // Also read below, once mutex-free, to keep eco mode from delaying the finish check (and
        // its melody) by sleeping through the last minute of the shift - see its use further down.
        bool finish_imminent = false;
        // Only meaningful while running: ms until the next thing tied to the digital label's own
        // minute clock needs a redraw - either the label's displayed minute changing, or the
        // animation blip (synced to that same event, see eco_blip_active) closing back to frame 1
        // a second later. Lets eco mode wake exactly then instead of on a fixed interval that
        // drifts out of phase with it. See its use further down.
        uint32_t ms_to_next_minute = ECO_FRAME_MS;

        if(furi_mutex_acquire(app->mutex, 0) == FuriStatusOk) {
            if(app->has_been_started && app->running && !app->finish_sound_played) {
                uint32_t timer_duration_seconds = app->cfg.timer_duration_hours * 3600;
                uint32_t elapsed_ms = current_tick - app->start_tick;
                uint32_t elapsed_seconds = app->elapsed_seconds + (elapsed_ms / 1000);
                uint32_t total_elapsed_ms = app->elapsed_seconds * 1000 + elapsed_ms;
                uint32_t ms_into_minute = total_elapsed_ms % 60000;
                // The label (and the blip, synced to it) only actually flips once elapsed is
                // strictly past a multiple of 60000 - landing exactly on it (ms_into_minute == 0)
                // still shows the old value, so that case needs only 1ms more, not a fresh 60s
                // wait. Once flipped, ms_into_minute is 1..999 while the blip window is still
                // open; wake when it closes rather than waiting a full minute for the next label
                // change.
                if(ms_into_minute == 0) {
                    ms_to_next_minute = 1;
                } else if(ms_into_minute < 1000) {
                    ms_to_next_minute = 1000 - ms_into_minute;
                } else {
                    ms_to_next_minute = 60001 - ms_into_minute;
                }
                uint32_t remaining = timer_duration_seconds > elapsed_seconds ?
                                         timer_duration_seconds - elapsed_seconds :
                                         0;
                finish_imminent = remaining <= FINISH_CHECK_THRESHOLD;

                // Check finish first (before hour check) to prioritize finish sound
                // Only check finish frequently when close (battery optimization)
                if(remaining <= FINISH_CHECK_THRESHOLD) {
                    // Check every FINISH_CHECK_INTERVAL ms when close
                    if((current_tick - last_finish_check) >= FINISH_CHECK_INTERVAL) {
                        should_check_finish = true;
                        last_finish_check = current_tick;
                    }
                } else {
                    // Check less frequently when far from finish
                    if((current_tick - last_finish_check) >= 5000) { // Every 5 seconds
                        should_check_finish = true;
                        last_finish_check = current_tick;
                    }
                }

                if(should_check_finish) {
                    float progress = (elapsed_seconds * 1000.0f + (elapsed_ms % 1000)) /
                                     (timer_duration_seconds * 1000.0f);

                    if(progress >= 1.0f) {
                        // Accumulate final elapsed time and cap at timer duration
                        app->elapsed_seconds += (elapsed_ms / 1000);
                        app->ms_adjust = elapsed_ms % 1000;
                        if(app->elapsed_seconds > timer_duration_seconds) {
                            app->elapsed_seconds = timer_duration_seconds;
                            app->ms_adjust = 0;
                        }
                        app->finish_sound_played = true;
                        app->finish_wallclock_secs = rtc_now_seconds();
                        app->running = false;
                        bool sound_enabled = app->cfg.sound_enabled;
                        bool vibro_enabled = app->cfg.vibro_enabled;
                        furi_mutex_release(app->mutex);
                        // Force update to show finished state
                        view_port_update(view_port);
                        // Play sound/light/vibro outside of mutex - light and vibro still fire
                        // even if muted
                        play_rick_roll_melody(notification, sound_enabled, vibro_enabled);
                        // Re-acquire mutex for next iteration
                        continue;
                    }
                }

                // Check for hour milestones more frequently (every second)
                // Only check if not finished (remaining > 0) to avoid playing chime at finish
                // This ensures timely chime playback without significant battery impact
                if(remaining > 0 &&
                   (current_tick - last_hour_check) >= 1000) { // Check every second
                    last_hour_check = current_tick;
                    uint32_t current_hour = elapsed_seconds / 3600;
                    if(current_hour > 0 && current_hour != app->last_hour_played) {
                        app->last_hour_played = current_hour;
                        bool sound_enabled = app->cfg.sound_enabled;
                        bool vibro_enabled = app->cfg.vibro_enabled;
                        furi_mutex_release(app->mutex);
                        // Play hour chime outside of mutex - light and vibro still fire even if
                        // muted
                        play_hour_chime(notification, sound_enabled, vibro_enabled);
                        // Re-acquire mutex for next iteration
                        continue;
                    }
                }
            }
            furi_mutex_release(app->mutex);
        }

        // Redraw cadence, cheapest case first:
        // - A hold's progress bar is filling: redraw fast, in any screen/mode, so it looks smooth.
        // - The info pager: nothing on it animates, so only user input should wake this loop.
        // - The shift is about to finish: force the same fast cadence the finish-check above
        //   needs, regardless of eco mode - otherwise an eco-frozen, idle shift only wakes up
        //   once a minute and the finish melody/state can land up to a minute late.
        // - Everything else (Set mode and a finished shift included): the dial (or, once
        //   finished, just the mascot's sleep animation - the dial itself stays frozen) ticks in
        //   real time, so redraw at 1Hz, unless eco mode has slowed things down after a period of
        //   inactivity - in which case it instead wakes right when something frozen is about to
        //   visibly change (the countdown's minute digit, and/or the once-a-minute animation blip).
        bool hold_in_progress = (app->ok_press_tick != 0 && !app->ok_hold_triggered) ||
                                (app->back_press_tick != 0 && !app->back_hold_triggered);
        uint32_t frame_interval;
        uint32_t queue_timeout;
        if(hold_in_progress) {
            frame_interval = HOLD_PROGRESS_FRAME_MS;
            queue_timeout = frame_interval;
        } else if(app->screen == ScreenInfo) {
            frame_interval = 0; // unused - queue_timeout never times out, so this never gets read
            queue_timeout = FuriWaitForever;
        } else if(finish_imminent) {
            frame_interval = FINISH_CHECK_INTERVAL;
            queue_timeout = frame_interval;
        } else {
            bool eco_frozen = app->cfg.eco_mode_enabled &&
                              (current_tick - app->last_activity_tick) >= ECO_IDLE_MS;
            if(eco_frozen) {
                if(app->running) {
                    // ms_to_next_minute already covers both events tied to the label's own
                    // minute clock: the digit changing, and the blip (synced to it) closing a
                    // second later - so the animation never drifts out of phase with the label.
                    frame_interval = ms_to_next_minute;
                } else {
                    // Nothing here has a shift-elapsed clock to sync to (Set mode's duration is
                    // static, a break's countdown is frozen, a finished shift's dial is frozen) -
                    // the blip instead runs on real wall-clock time as an ambient "still alive"
                    // tick. rtc_now_seconds() only has 1s resolution, so this can land up to ~1s
                    // early - harmless here, unlike the countdown digit, since it just costs one
                    // extra recheck rather than showing a wrong value.
                    uint32_t sec_in_minute = rtc_now_seconds() % 60;
                    frame_interval = sec_in_minute == 0 ? 1000 : (60 - sec_in_minute) * 1000;
                }
            } else {
                frame_interval = FRAME_MS_RUNNING;
            }
            queue_timeout = frame_interval;
        }

        if(furi_message_queue_get(event_queue, &event, queue_timeout) == FuriStatusOk) {
            app->last_activity_tick = furi_get_tick();
            if(event.key == InputKeyBack && event.sequence_source == INPUT_SEQUENCE_SOURCE_SOFTWARE) {
                // A software/RPC-injected event (e.g. the Loader's "close app" request from
                // `ufbt launch` or the app_close CLI command) - close immediately regardless
                // of type, bypassing the hold-to-confirm meant for real button presses.
                terminate = true;
            } else if(event.type == InputTypePress) {
                // The combo's last key is also the debug-action key - don't let the press that
                // unlocks debug mode also immediately trigger that action.
                bool debug_just_unlocked =
                    app->has_been_started && debug_feed_combo_key(event.key);
                if(debug_just_unlocked) {
                    play_rick_roll_melody(
                        notification, app->cfg.sound_enabled, app->cfg.vibro_enabled);
                }
                switch(event.key) {
                case InputKeyOk:
                    // OK does nothing on the info pager - Back is still how you leave it.
                    if(furi_mutex_acquire(app->mutex, 100) == FuriStatusOk) {
                        if(app->screen != ScreenInfo) {
                            app->ok_press_tick = furi_get_tick();
                            app->ok_hold_triggered = false;
                        }
                        furi_mutex_release(app->mutex);
                    }
                    break;
                case InputKeyBack:
                    if(furi_mutex_acquire(app->mutex, 100) == FuriStatusOk) {
                        app->back_press_tick = furi_get_tick();
                        app->back_hold_triggered = false;
                        furi_mutex_release(app->mutex);
                    }
                    break;
                case InputKeyUp:
                case InputKeyDown:
                    // Set Shift mode: Up/Down instead switch the time format - that's the only
                    // screen the format can be changed from. No function on the info pager.
                    if(furi_mutex_acquire(app->mutex, 100) == FuriStatusOk) {
                        if(app->screen == ScreenInfo) {
                            // No function here
                        } else if(!app->has_been_started) {
                            app->cfg.long_time_format = !app->cfg.long_time_format;
                            cfg_save_internal(file, &app->cfg);
                        } else if(event.key == InputKeyUp) {
                            // Mute toggle works the same in every mode. Same reveal-then-confirm
                            // pattern as backlight/eco - first press while its icon isn't showing
                            // just reveals the current state. While muted, sound-off is always
                            // showing, so a press then confirms (unmutes) immediately, same as
                            // any other press aimed at an icon that's already on screen.
                            uint32_t now = furi_get_tick();
                            bool icon_visible =
                                current_top_right_icon_slot(app, now) == TopRightIconSound;
                            if(icon_visible) {
                                app->cfg.sound_enabled = !app->cfg.sound_enabled;
                                cfg_save_internal(file, &app->cfg);
                            }
                            app->sound_state_change_tick = now;
                        } else {
                            // Backlight toggle works the same in every mode. The first press
                            // while its icon isn't showing just reveals the current state; press
                            // again while it's showing to actually change it.
                            uint32_t now = furi_get_tick();
                            bool icon_visible =
                                current_top_right_icon_slot(app, now) == TopRightIconBacklight;
                            if(icon_visible) {
                                app->cfg.backlight_on = !app->cfg.backlight_on;
                                set_backlight(notification, app->cfg.backlight_on);
                                cfg_save_internal(file, &app->cfg);
                            }
                            app->backlight_state_change_tick = now;
                        }
                        furi_mutex_release(app->mutex);
                    }
                    break;
                case InputKeyLeft:
                case InputKeyRight:
                    if(furi_mutex_acquire(app->mutex, 100) == FuriStatusOk) {
                        if(app->screen == ScreenInfo) {
                            // Info pager: left/right cycle between pages
                            if(event.key == InputKeyRight) {
                                app->info_page = (app->info_page + 1) % INFO_PAGE_COUNT;
                            } else {
                                app->info_page =
                                    (app->info_page + INFO_PAGE_COUNT - 1) % INFO_PAGE_COUNT;
                            }
                        } else if(!app->has_been_started) {
                            // Shift mode: adjust the configured shift length
                            if(event.key == InputKeyRight) {
                                modify_timer_up(&app->cfg);
                            } else {
                                modify_timer_down(&app->cfg);
                            }
                            cfg_save_internal(file, &app->cfg);
                        } else if(event.key == InputKeyLeft) {
                            if(debug_just_unlocked) {
                                // Consumed entirely by the unlock celebration
                            } else if(is_debug_mode_active()) {
                                // Left fast-forwards; holding it ramps up via the Repeat handler
                                // below, using this tick as the hold's start for that ramp.
                                app->debug_travel_press_tick = furi_get_tick();
                                debug_time_travel(app, 1);
                            } else {
                                // Working/Break mode: toggle vibro. Same reveal-then-confirm
                                // pattern as eco/backlight - first press just shows the
                                // current state.
                                uint32_t now = furi_get_tick();
                                bool icon_visible =
                                    current_top_right_icon_slot(app, now) == TopRightIconVibro;
                                if(icon_visible) {
                                    app->cfg.vibro_enabled = !app->cfg.vibro_enabled;
                                    cfg_save_internal(file, &app->cfg);
                                }
                                app->vibro_state_change_tick = now;
                            }
                        } else if(event.key == InputKeyRight) {
                            if(is_debug_mode_active()) {
                                // Right rewinds - the mirror image of Left's fast-forward.
                                app->debug_travel_press_tick = furi_get_tick();
                                debug_time_travel(app, -1);
                            } else {
                                // Working/Break mode: toggle eco mode. Same reveal-then-confirm
                                // pattern as backlight - first press just shows the current state.
                                uint32_t now = furi_get_tick();
                                bool icon_visible =
                                    current_top_right_icon_slot(app, now) == TopRightIconEco;
                                if(icon_visible) {
                                    app->cfg.eco_mode_enabled = !app->cfg.eco_mode_enabled;
                                    cfg_save_internal(file, &app->cfg);
                                }
                                app->eco_state_change_tick = now;
                            }
                        }
                        furi_mutex_release(app->mutex);
                    }
                    break;
                default:
                    break;
                }
            } else if(event.type == InputTypeRepeat) {
                switch(event.key) {
                case InputKeyOk:
                    if(furi_mutex_acquire(app->mutex, 100) == FuriStatusOk) {
                        if(app->ok_press_tick != 0 && !app->ok_hold_triggered) {
                            uint32_t required = app->has_been_started ? HOLD_CONFIRM_MS :
                                                                         INFO_HOLD_MS;
                            if((furi_get_tick() - app->ok_press_tick) >= required) {
                                if(app->has_been_started) {
                                    // Held long enough - reset back to Shift mode
                                    reset_to_shift_mode(app);
                                } else {
                                    // Held long enough in Set mode - open the info/options pager
                                    app->screen = ScreenInfo;
                                    app->info_page = 0;
                                }
                                app->ok_hold_triggered = true;
                            }
                        }
                        furi_mutex_release(app->mutex);
                    }
                    break;
                case InputKeyBack:
                    if(furi_mutex_acquire(app->mutex, 100) == FuriStatusOk) {
                        if(app->back_press_tick != 0 && !app->back_hold_triggered &&
                           (furi_get_tick() - app->back_press_tick) >= back_hold_required_ms(app)) {
                            app->back_hold_triggered = true;
                            terminate = true;
                        }
                        furi_mutex_release(app->mutex);
                    }
                    break;
                case InputKeyLeft:
                case InputKeyRight:
                    if(app->has_been_started) {
                        // Holding left/right ramps the debug time-travel step up the longer
                        // it's held, for fast bulk jumps; does nothing outside debug mode.
                        if(is_debug_mode_active() &&
                           furi_mutex_acquire(app->mutex, 100) == FuriStatusOk) {
                            uint32_t held_ms = furi_get_tick() - app->debug_travel_press_tick;
                            int32_t step = debug_travel_step_minutes(held_ms);
                            debug_time_travel(app, event.key == InputKeyLeft ? step : -step);
                            furi_mutex_release(app->mutex);
                        }
                    } else {
                        // Holding left/right keeps cycling the shift duration in Shift mode
                        adjust_shift_duration(app, file, event.key == InputKeyRight);
                    }
                    break;
                default:
                    break;
                }
            } else if(event.type == InputTypeRelease) {
                switch(event.key) {
                case InputKeyOk:
                    if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
                        uint32_t hold_ms =
                            app->ok_press_tick != 0 ? furi_get_tick() - app->ok_press_tick : 0;
                        // Only a plain tap (released before the reset overlay ever showed)
                        // starts/pauses/resumes. Releasing mid-hold just cancels the reset,
                        // same as reaching HOLD_CONFIRM_MS already did via the repeat handler.
                        if(!app->ok_hold_triggered && app->ok_press_tick != 0 &&
                           hold_ms < HOLD_SHOW_MS) {
                            uint32_t timer_duration_seconds =
                                app->cfg.timer_duration_hours * 3600;
                            uint32_t now_tick = furi_get_tick();
                            uint32_t elapsed_seconds = app->elapsed_seconds;
                            if(app->running) {
                                uint32_t elapsed_ms = now_tick - app->start_tick;
                                elapsed_seconds += (elapsed_ms / 1000);
                            }
                            float progress = (elapsed_seconds * 1000.0f + app->ms_adjust) /
                                             (timer_duration_seconds * 1000.0f);
                            bool is_finished = (progress >= 1.0f);

                            if(is_finished) {
                                // Timer finished: reset to Shift mode
                                reset_to_shift_mode(app);
                            } else if(app->running) {
                                if(app->break_count >= MAX_BREAKS) {
                                    // Every break slot is already used - refuse to start another
                                    // one (keep running) and say so, rather than letting the
                                    // break happen and only discovering the cap when it ends.
                                    app->break_limit_message_tick = now_tick;
                                } else {
                                    // Stop timer - accumulate elapsed time including milliseconds
                                    uint32_t elapsed_ms = now_tick - app->start_tick;
                                    app->running = false;

                                    if(app->pause_start_tick != 0 && elapsed_ms < BREAK_FOLD_MS) {
                                        // Too little work happened since the last break to credit
                                        // as work, same threshold as folding a break. The anchor
                                        // still moves to now, though: the break before this sliver
                                        // was already folded/logged at the resume that started it,
                                        // so leaving the anchor behind would re-span and
                                        // double-count that already-committed time on the next
                                        // resume.
                                        app->pause_start_tick = now_tick;
                                        app->pause_start_wallclock = rtc_now_seconds();
                                    } else {
                                        app->elapsed_seconds += (elapsed_ms / 1000);
                                        app->ms_adjust =
                                            elapsed_ms % 1000; // Store remaining milliseconds
                                        app->pause_start_tick = now_tick;
                                        app->pause_start_wallclock = rtc_now_seconds();
                                    }
                                }
                            } else {
                                bool is_resume = app->has_been_started;
                                if(!is_resume) {
                                    // Record the real shift-start time only on the very first
                                    // start; resuming after a break keeps the original dial anchor
                                    app->start_wallclock_secs = rtc_now_seconds();
                                } else {
                                    // Breaks under a minute aren't worth logging - fold them
                                    // straight into worked time instead. Longer ones get logged
                                    // with their real start/end so the dial can show them. The
                                    // break cap is enforced when a break starts (above), so there
                                    // is always room left to log one that's actually in progress.
                                    uint32_t break_ms = now_tick - app->pause_start_tick;
                                    if(break_ms < BREAK_FOLD_MS) {
                                        uint32_t ms_total = app->ms_adjust + (break_ms % 1000);
                                        app->elapsed_seconds +=
                                            break_ms / 1000 + ms_total / 1000;
                                        app->ms_adjust = ms_total % 1000;
                                    } else {
                                        app->breaks[app->break_count].start_wallclock_secs =
                                            app->pause_start_wallclock;
                                        app->breaks[app->break_count].end_wallclock_secs =
                                            rtc_now_seconds();
                                        app->break_count++;
                                    }
                                }
                                // Start timer - adjust start_tick to account for stored milliseconds
                                app->has_been_started = true;
                                // Subtract stored milliseconds from start_tick so timer continues from where it paused
                                app->start_tick = furi_get_tick() - app->ms_adjust;
                                app->ms_adjust = 0; // Reset milliseconds adjustment
                                app->running = true;
                                app->finish_sound_played = false; // Reset sound flag when starting
                            }
                        }
                        app->ok_press_tick = 0;
                        furi_mutex_release(app->mutex);
                    }
                    break;
                case InputKeyBack:
                    // Releasing before the hold completes just cancels the close - on the info
                    // screen, a plain short tap instead returns to the clock (already in Set
                    // mode, since that's the only mode the info screen is reachable from); at a
                    // finished shift, a tap exits it too, same as OK.
                    if(furi_mutex_acquire(app->mutex, 100) == FuriStatusOk) {
                        if(!app->back_hold_triggered) {
                            if(app->screen == ScreenInfo) {
                                app->screen = ScreenClock;
                            } else if(app->finish_sound_played) {
                                reset_to_shift_mode(app);
                            }
                        }
                        app->back_press_tick = 0;
                        furi_mutex_release(app->mutex);
                    }
                    break;
                default:
                    break;
                }
            }
            // Force update after input events (user interaction requires immediate feedback)
            view_port_update(view_port);
        } else {
            // The real-time hand (and, while paused, the growing gap) needs to tick every
            // second no matter the timer state - unless eco mode has slowed things down above.
            static uint32_t last_update = 0;
            uint32_t now = furi_get_tick();

            if((now - last_update) >= frame_interval) {
                view_port_update(view_port);
                last_update = now;
            }
        }
    }

    notification_message_block(notification, &sequence_display_backlight_enforce_auto);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_message_queue_free(event_queue);
    view_port_free(view_port);
    storage_file_free(file);

    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
