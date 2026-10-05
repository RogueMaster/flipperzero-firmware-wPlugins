#pragma once
#include <furi.h>
#include <gui/gui.h>

#define OFS_LEFT_X          31
#define FACE_RADIUS         31
#define MAX_TIMER_HOURS     12 // Max configurable shift length, in hours
#define DEFAULT_TIMER_HOURS 8

// How long the OK/Back button must be held before it triggers reset/close. Closing from Set
// mode (nothing running yet, so nothing to lose) only needs half as long a hold.
#define HOLD_CONFIRM_MS        3000
// Holding OK in Set mode instead opens the info/options pager - needs only a quarter of the hold
#define INFO_HOLD_MS           (HOLD_CONFIRM_MS / 4)
// Hold overlay (progress bar + label) only appears once the hold has run this long
#define HOLD_SHOW_MS           300
// How long the sound-on and backlight icons flash after their state changes
#define ICON_FLASH_MS          5000
// Breaks shorter than this are folded into worked time instead of leaving a visible gap
#define BREAK_FOLD_MS          60000
// Max number of individually-logged breaks per shift; further breaks past this just aren't logged
#define MAX_BREAKS             16
// How long the "too many breaks" message shows after a break is dropped for hitting that cap
#define BREAK_LIMIT_MESSAGE_MS 5000
// Eco mode: after this long with no button pressed, redraws slow down and animations freeze
#define ECO_IDLE_MS            60000
// Eco mode's slow redraw interval once idle (vs. the normal 1-per-second rate)
#define ECO_FRAME_MS           60000

// Segment fill geometry: a square inset from the dial's edge, sized so the frame stays uncovered.
#define FILL_MARGIN         5
#define FILL_HALF_SIZE      (FACE_RADIUS - FILL_MARGIN)
#define FILL_GRID_SIDE      (2 * FILL_HALF_SIZE + 1)
// Half the pixels in the fill square pass the worked-pattern's dither (only those are stored)
#define FILL_PIXEL_CAPACITY ((FILL_GRID_SIDE * FILL_GRID_SIDE + 1) / 2)

typedef enum {
    Normal = 0,
    Thick
} LineType;

typedef struct {
    int8_t x;
    int8_t y;
} Point;

typedef struct {
    Point start;
    Point end;
} Line;

// A pixel eligible for the worked-segment dither, with its angle on the dial precomputed once
// at startup instead of every frame - only its arc containment needs checking per redraw.
typedef struct {
    int8_t x;
    int8_t y;
    float angle;
} FillPixel;

typedef struct {
    FillPixel fill_pixels[FILL_PIXEL_CAPACITY];
    uint16_t fill_pixel_count;
} ClockFace;

#define CONFIG_VERSION 13
typedef struct {
    uint8_t version;
    uint8_t timer_duration_hours; // Shift duration in hours (1-12)
    bool sound_enabled; // Hour chime + finish melody on/off
    bool backlight_on; // Manual backlight toggle, available in every mode
    bool eco_mode_enabled; // Slow down + freeze animations after a minute of inactivity
    bool vibro_enabled; // Vibrate in sync with hour chime / finish melody
    bool long_time_format; // Show H:MM:SS instead of H:MM; only changeable in Set Shift mode
} TimerConfig;

// Sound, eco, backlight, and vibro icons all share one top-right slot; only one shows at a
// time, whichever changed most recently (sound-off is the standing default while muted, since
// it never expires on its own - anything else showing more recently still takes priority though).
typedef enum {
    TopRightIconNone,
    TopRightIconSound,
    TopRightIconEco,
    TopRightIconBacklight,
    TopRightIconVibro,
} TopRightIconSlot;

// Chrome/overlay state that isn't part of the clock's own timekeeping, prepared by the app
// and handed to the renderer each frame.
typedef struct {
    bool sound_enabled;
    bool backlight_on;
    bool eco_mode_enabled;
    bool vibro_enabled;
    bool long_time_format;
    TopRightIconSlot top_right_icon_slot;
    bool animations_frozen; // eco mode has kicked in after a minute idle - hold frame 1/dashed secs
    bool eco_blip_active; // frozen only: real-time minute just ticked over - show frame 2 for 1s
    bool hold_active; // a qualifying reset/close hold is in progress, past HOLD_SHOW_MS
    float hold_fraction; // 0..1 linear progress toward HOLD_CONFIRM_MS (eased at draw time)
    const char* hold_label; // "RESETTING" or "CLOSING"
    bool break_limit_message; // a break couldn't start because MAX_BREAKS is used up - say so
} UiOverlay;

// A single completed break, logged only once it's run at least BREAK_FOLD_MS.
typedef struct {
    uint32_t start_wallclock_secs;
    uint32_t end_wallclock_secs;
} BreakInterval;

// The break history for the current shift, handed to the renderer so it can carve each break
// out of the worked arc individually instead of showing one lumped trailing gap.
typedef struct {
    const BreakInterval* items;
    uint8_t count;
    bool live_active; // true if currently on a break that hasn't been logged yet (still ongoing)
    uint32_t live_start_wallclock_secs; // valid only when live_active
} BreakLog;

void calc_clock_face(ClockFace* face);
void draw_timer(
    Canvas* canvas,
    ClockFace* face,
    uint8_t timer_duration_hours,
    uint32_t elapsed_seconds,
    uint16_t ms,
    bool running,
    bool has_been_started,
    uint32_t now_wallclock_secs, // frozen once finished, so the dial itself stops moving
    uint32_t animation_wallclock_secs, // always real time, so the mascot keeps animating
    uint32_t start_wallclock_secs,
    const BreakLog* break_log,
    const UiOverlay* ui);

void init_timer_config(TimerConfig* cfg);
void modify_timer_up(TimerConfig* cfg);
void modify_timer_down(TimerConfig* cfg);
