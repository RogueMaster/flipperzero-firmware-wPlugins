/*
 * ServoMotor - Flipper Zero servo tester with hardware PWM and an animated servo.
 *
 * Pulse generation uses TIM1 on PA7 (pin 2): fixed 50 Hz frame, variable pulse width,
 * so Flipper background tasks cannot stretch or shift the pulses.
 *
 * Controls:
 *   OK          cycle mode (Manual -> Center -> Sweep)
 *   Left/Right  -/+ 10 degrees (Manual mode, main page)
 *   Down/Up     next/previous page (Main, Wiring, Pulse info, Safety)
 *   Back        return to main page / exit
 *
 * UI derived from Servo Tester+ (MIT); redesigned with an animated servo dial,
 * tabbed modes and graphical help pages.
 */

#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_pwm.h>
#include <stm32wbxx_ll_tim.h>
#include <gui/gui.h>
#include <input/input.h>
#include <applications/services/power/power_service/power.h>

#include <math.h>
#include <stdio.h>

#define SERVO_ANGLE_MIN    0
#define SERVO_ANGLE_MAX    180
#define SERVO_ANGLE_STEP   10
#define SERVO_PULSE_MIN_US 1000
#define SERVO_PULSE_MAX_US 2000
#define SWEEP_DWELL_MS     2000

/* Simulated horn speed on screen: degrees*10 per animation tick (30 ms). */
#define ANIM_TICK_MS  30
#define ANIM_STEP_X10 70

/* Hardware PWM on TIM1 (PA7, pin 2). Timer clock 64 MHz.
 * Prescaler 19 -> 3.2 MHz tick (0.3125 us). Auto-reload 63999 -> 20 ms frame (50 Hz).
 * Pulse width in ticks = pulse_us * 3.2 = pulse_us * 16 / 5. */
#define SERVO_PWM_PRESCALER   19
#define SERVO_PWM_AUTORELOAD  63999
#define SERVO_US_TO_TICKS(us) (((uint32_t)(us) * 16U) / 5U)

/* Dial geometry (pixels). */
#define DIAL_CX         38
#define DIAL_CY         44
#define DIAL_R_HORN     22
#define DIAL_R_TICK_IN  25
#define DIAL_R_TICK_OUT 29
#define DIAL_R_TARGET   32

typedef enum {
    ServoModeManual,
    ServoModeCenter,
    ServoModeSweep,
    ServoModeCount,
} ServoMode;

typedef enum {
    PageMain,
    PageWiring,
    PagePulse,
    PageSafety,
    PageCount,
} Page;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    FuriTimer* sweep_timer;
    FuriTimer* anim_timer;
    ViewPort* view_port;
    Gui* gui;
    Power* power;
    uint16_t pulse_us;
    uint8_t angle; /* commanded angle */
    int16_t shown_x10; /* simulated horn angle, degrees * 10 */
    ServoMode mode;
    Page page;
    bool running;
    bool otg_was_enabled;
    bool otg_enabled_by_app;
    bool five_volts_available;
} ServoMotorApp;

/* Rounded to the nearest microsecond: 1 degree = 5.56 us. */
static uint16_t angle_to_pulse(uint8_t angle) {
    return SERVO_PULSE_MIN_US +
           ((SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US) * angle + SERVO_ANGLE_MAX / 2) /
               SERVO_ANGLE_MAX;
}

/* ---------------------------------------------------------------- PWM */

static void servo_pwm_set_pulse(uint16_t pulse_us) {
    LL_TIM_SetPrescaler(TIM1, SERVO_PWM_PRESCALER);
    LL_TIM_SetAutoReload(TIM1, SERVO_PWM_AUTORELOAD);
    LL_TIM_OC_SetCompareCH1(TIM1, SERVO_US_TO_TICKS(pulse_us));
}

static void servo_pwm_start(uint16_t pulse_us) {
    /* Initializes TIM1 and routes it to PA7; the LL calls then set exact timing. */
    furi_hal_pwm_start(FuriHalPwmOutputIdTim1PA7, 50, 5);
    servo_pwm_set_pulse(pulse_us);
}

static void servo_pwm_stop(void) {
    furi_hal_pwm_stop(FuriHalPwmOutputIdTim1PA7);
}

/* ---------------------------------------------------- drawing helpers */

/* Point at distance r from (cx,cy). 0 deg = left, 90 = up, 180 = right. */
static void polar(int cx, int cy, float r, float deg, int* x, int* y) {
    float rad = deg * (float)M_PI / 180.0f;
    *x = cx - (int)roundf(r * cosf(rad));
    *y = cy - (int)roundf(r * sinf(rad));
}

/* Small solid arrow pointing down or up, centred on x. */
static void draw_arrow_v(Canvas* canvas, int x, int y, bool down) {
    for(int i = 0; i < 3; i++) {
        int row = down ? y + i : y + 2 - i;
        canvas_draw_line(canvas, x - 2 + i, row, x + 2 - i, row);
    }
}

/* Inverted title bar with page number, e.g. "WIRING 2/4". */
static void draw_title(Canvas* canvas, const char* title, Page page) {
    char num[8];
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_box(canvas, 0, 0, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 3, 8, title);
    snprintf(num, sizeof(num), "%d/%d", page + 1, PageCount);
    canvas_draw_str_aligned(canvas, 125, 8, AlignRight, AlignBottom, num);
    /* Up/down arrows: pages are browsed with the arrow keys. */
    int ax = 125 - canvas_string_width(canvas, num) - 5;
    draw_arrow_v(canvas, ax, 1, false);
    draw_arrow_v(canvas, ax, 5, true);
    canvas_set_color(canvas, ColorBlack);
}

/* ---------------------------------------------------------- main page */

#define HORN_TIP_R     18.0f /* distance from shaft to the centre of the tip hole */
#define HORN_HUB_R     6 /* clamp hub radius */
#define HORN_W_HUB     3.5f /* arm half-width where it leaves the hub */
#define HORN_W_TIP     2.8f /* arm half-width at the tip */
#define HORN_TIP_ROUND 3 /* rounded tip radius */

static int iround(float v) {
    return (int)roundf(v);
}

/* Single-arm clamp horn: big round hub with splined hole and clamp slit,
 * tapered arm and a rounded tip with the linkage hole. Rotates with deg. */
static void draw_horn(Canvas* canvas, float deg) {
    float rad = deg * (float)M_PI / 180.0f;
    float dx = -cosf(rad), dy = -sinf(rad); /* unit vector shaft -> tip */
    float px = -dy, py = dx; /* perpendicular */
    float tx = DIAL_CX + dx * HORN_TIP_R, ty = DIAL_CY + dy * HORN_TIP_R;

    /* Solid tapered arm, filled with closely spaced lines. */
    for(float t = -1.0f; t <= 1.0f; t += 0.1f) {
        canvas_draw_line(
            canvas,
            iround(DIAL_CX + px * HORN_W_HUB * t),
            iround(DIAL_CY + py * HORN_W_HUB * t),
            iround(tx + px * HORN_W_TIP * t),
            iround(ty + py * HORN_W_TIP * t));
    }
    canvas_draw_disc(canvas, DIAL_CX, DIAL_CY, HORN_HUB_R);
    canvas_draw_disc(canvas, iround(tx), iround(ty), HORN_TIP_ROUND);

    canvas_set_color(canvas, ColorWhite);
    /* Splined output hole. */
    canvas_draw_disc(canvas, DIAL_CX, DIAL_CY, 2);
    /* Clamp slit: from the hole to the hub rim, opposite the arm. */
    canvas_draw_line(
        canvas,
        iround(DIAL_CX - dx * 3),
        iround(DIAL_CY - dy * 3),
        iround(DIAL_CX - dx * HORN_HUB_R),
        iround(DIAL_CY - dy * HORN_HUB_R));
    /* Linkage hole at the tip. */
    canvas_draw_disc(canvas, iround(tx), iround(ty), 1);
    canvas_set_color(canvas, ColorBlack);
    /* Spline centre. */
    canvas_draw_dot(canvas, DIAL_CX, DIAL_CY);
}

/* Servo seen from above: rectangular case with the output shaft near the top end,
 * a round raised housing around the shaft and mounting tabs with screw holes. */
static void draw_servo_body(Canvas* canvas) {
    const int x = DIAL_CX - 8, w = 17; /* case */
    const int y = DIAL_CY - 8, h = 22;

    /* Mounting tabs at both ends, with screw holes. */
    canvas_draw_frame(canvas, x - 2, y - 4, w + 4, 5);
    canvas_draw_frame(canvas, x - 2, y + h - 1, w + 4, 5);
    canvas_draw_dot(canvas, x + 1, y - 2);
    canvas_draw_dot(canvas, x + w - 2, y - 2);
    canvas_draw_dot(canvas, x + 1, y + h + 1);
    canvas_draw_dot(canvas, x + w - 2, y + h + 1);

    /* Case (cleared inside so the tab lines do not show through). */
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, y, w, h);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, x, y, w, h, 2);

    /* Raised gear housing around the output shaft. */
    canvas_draw_circle(canvas, DIAL_CX, DIAL_CY, 7);

    /* Small detail at the lower end: label plate. */
    canvas_draw_line(canvas, x + 4, y + h - 5, x + w - 5, y + h - 5);
    canvas_draw_line(canvas, x + 4, y + h - 3, x + w - 5, y + h - 3);
}

static void draw_dial(Canvas* canvas, float shown_deg, int target_deg) {
    int x1, y1, x2, y2;

    /* Scale: long ticks every 30 degrees, dots every 10. */
    for(int a = 0; a <= 180; a += 10) {
        if(a % 30 == 0) {
            polar(DIAL_CX, DIAL_CY, DIAL_R_TICK_IN, a, &x1, &y1);
            polar(DIAL_CX, DIAL_CY, DIAL_R_TICK_OUT, a, &x2, &y2);
            canvas_draw_line(canvas, x1, y1, x2, y2);
        } else {
            polar(DIAL_CX, DIAL_CY, DIAL_R_TICK_OUT - 1, a, &x1, &y1);
            canvas_draw_dot(canvas, x1, y1);
        }
    }

    /* Commanded angle marker, outside the scale. */
    polar(DIAL_CX, DIAL_CY, DIAL_R_TARGET, target_deg, &x1, &y1);
    canvas_draw_box(canvas, x1 - 1, y1 - 1, 3, 3);

    draw_servo_body(canvas);
    draw_horn(canvas, shown_deg);
}

static void draw_tabs(Canvas* canvas, ServoMode mode) {
    static const char* names[] = {"MANUAL", "CENTER", "SWEEP"};
    static const uint8_t xs[] = {0, 43, 86};
    static const uint8_t ws[] = {42, 42, 42};

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < ServoModeCount; i++) {
        if(mode == (ServoMode)i) {
            canvas_draw_rbox(canvas, xs[i], 0, ws[i], 11, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, xs[i] + ws[i] / 2, 6, AlignCenter, AlignCenter, names[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_main(Canvas* canvas, const ServoMotorApp* s) {
    char text[20];

    draw_tabs(canvas, s->mode);
    draw_dial(canvas, s->shown_x10 / 10.0f, s->angle);

    /* Right panel: 5 V status, commanded angle, pulse width. */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 126, 22, AlignRight, AlignBottom, s->five_volts_available ? "5V ON" : "5V ?");

    snprintf(text, sizeof(text), "%u", s->angle);
    canvas_set_font(canvas, FontBigNumbers);
    uint16_t w = canvas_string_width(canvas, text);
    canvas_draw_str_aligned(canvas, 99, 37, AlignCenter, AlignCenter, text);
    canvas_draw_circle(canvas, 99 + w / 2 + 4, 30, 2);

    canvas_set_font(canvas, FontSecondary);
    if(s->mode == ServoModeManual) {
        snprintf(text, sizeof(text), "< %u us >", s->pulse_us);
    } else {
        snprintf(text, sizeof(text), "%u us", s->pulse_us);
    }
    canvas_draw_str_aligned(canvas, 101, 53, AlignCenter, AlignBottom, text);

    /* Hint line, right of the servo: [OK] mode   up/down pages. */
    canvas_draw_rbox(canvas, 51, 55, 17, 9, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 54, 63, "OK");
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 70, 63, "mode");
    draw_arrow_v(canvas, 99, 55, false);
    draw_arrow_v(canvas, 99, 60, true);
    canvas_draw_str(canvas, 103, 63, "pages");
}

/* --------------------------------------------------------- wiring page */

#define HDR_Y        12
#define HDR_PIN_X(n) (7 + 13 * ((n) - 1)) /* centre x of header pin n (1..9) */
#define BADGE_Y      50

/* One highlighted connection: pin -> routed wire -> function badge. */
static void draw_badge(Canvas* canvas, int x, int w, const char* text) {
    canvas_draw_rbox(canvas, x, BADGE_Y, w, 13, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, x + w / 2, BADGE_Y + 7, AlignCenter, AlignCenter, text);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
}

/* Two-pixel wide wire. */
static void draw_wire(Canvas* canvas, int x1, int y1, int x2, int y2) {
    canvas_draw_line(canvas, x1, y1, x2, y2);
    if(x1 == x2) {
        canvas_draw_line(canvas, x1 + 1, y1, x2 + 1, y2);
    } else {
        canvas_draw_line(canvas, x1, y1 + 1, x2, y2 + 1);
    }
}

static void draw_wiring(Canvas* canvas, Page page) {
    char num[3];

    draw_title(canvas, "WIRING  top header", page);

    /* GPIO header pins 1..9: used pins solid with their number, others hollow. */
    canvas_draw_rframe(canvas, 0, HDR_Y - 1, 120, 12, 2);
    for(int n = 1; n <= 9; n++) {
        int cx = HDR_PIN_X(n);
        if(n == 1 || n == 2 || n == 8) {
            canvas_draw_box(canvas, cx - 4, HDR_Y + 1, 9, 9);
            canvas_set_color(canvas, ColorWhite);
            snprintf(num, sizeof(num), "%d", n);
            canvas_draw_str_aligned(canvas, cx + 1, HDR_Y + 5, AlignCenter, AlignCenter, num);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_frame(canvas, cx - 2, HDR_Y + 3, 5, 5);
        }
    }

    /* Wires: pin 1 straight down, pin 2 jogs right, pin 8 jogs left. */
    int top = HDR_Y + 11;
    draw_wire(canvas, HDR_PIN_X(1), top, HDR_PIN_X(1), BADGE_Y - 1);
    draw_wire(canvas, HDR_PIN_X(2), top, HDR_PIN_X(2), 33);
    draw_wire(canvas, HDR_PIN_X(2), 33, 50, 33);
    draw_wire(canvas, 50, 33, 50, BADGE_Y - 1);
    draw_wire(canvas, HDR_PIN_X(8), top, HDR_PIN_X(8), 33);
    draw_wire(canvas, 92, 33, HDR_PIN_X(8), 33);
    draw_wire(canvas, 92, 33, 92, BADGE_Y - 1);

    /* Servo wire colour to the right of each wire. */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 11, 46, "red");
    canvas_draw_str(canvas, 54, 46, "yellow");
    canvas_draw_str(canvas, 96, 46, "brown");

    /* Function badges. */
    draw_badge(canvas, 0, 38, "+5V");
    draw_badge(canvas, 44, 38, "PWM");
    draw_badge(canvas, 88, 40, "GND");
}

/* ------------------------------------------------------ pulse info page */

static void draw_pulse_info(Canvas* canvas, Page page) {
    draw_title(canvas, "PULSE = POSITION", page);

    /* Waveform: pulse, long low gap (with break mark), next pulse. */
    const int base = 30, high = 16;
    canvas_draw_line(canvas, 2, base, 12, base);
    canvas_draw_line(canvas, 12, base, 12, high);
    canvas_draw_line(canvas, 12, high, 46, high);
    canvas_draw_line(canvas, 46, high, 46, base);
    canvas_draw_line(canvas, 46, base, 76, base);
    canvas_draw_line(canvas, 77, base + 2, 80, base - 2); /* break // */
    canvas_draw_line(canvas, 80, base + 2, 83, base - 2);
    canvas_draw_line(canvas, 84, base, 100, base);
    canvas_draw_line(canvas, 100, base, 100, high);
    canvas_draw_line(canvas, 100, high, 126, high);

    /* Pulse width label above the pulse, period label below. */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 30, 26, AlignCenter, AlignBottom, "1-2ms");
    canvas_draw_line(canvas, 12, 33, 12, 35);
    canvas_draw_line(canvas, 100, 33, 100, 35);
    canvas_draw_line(canvas, 12, 34, 44, 34);
    canvas_draw_line(canvas, 68, 34, 100, 34);
    canvas_draw_str_aligned(canvas, 56, 38, AlignCenter, AlignBottom, "20ms");

    canvas_draw_str(canvas, 1, 47, "Servo follows pulse width");
    canvas_draw_str(canvas, 1, 55, "0-180 = 1000-2000us,");
    canvas_draw_str(canvas, 1, 63, "travel depends on servo");
}

/* ----------------------------------------------------- safety info page */

static void draw_safety_info(Canvas* canvas, Page page) {
    draw_title(canvas, "SAFETY", page);
    canvas_set_font(canvas, FontSecondary);

    /* Warning sign. */
    canvas_draw_line(canvas, 10, 13, 2, 27);
    canvas_draw_line(canvas, 10, 13, 18, 27);
    canvas_draw_line(canvas, 2, 27, 18, 27);
    canvas_draw_line(canvas, 10, 17, 10, 22);
    canvas_draw_dot(canvas, 10, 25);

    canvas_draw_str(canvas, 23, 19, "Buzzing at an end =");
    canvas_draw_str(canvas, 23, 27, "end stop. Back off!");
    canvas_draw_str(canvas, 1, 39, "5V rail: max 1.2 A.");
    canvas_draw_str(canvas, 1, 47, "Big servo: external");
    canvas_draw_str(canvas, 1, 55, "5-6V supply, common GND.");
    canvas_draw_str(canvas, 1, 63, "PWM pin is 3.3V logic.");
}

/* ------------------------------------------------------- draw dispatch */

static void servo_motor_draw(Canvas* canvas, void* context) {
    ServoMotorApp* app = context;
    ServoMotorApp snapshot;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snapshot = *app;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    switch(snapshot.page) {
    case PageWiring:
        draw_wiring(canvas, snapshot.page);
        break;
    case PagePulse:
        draw_pulse_info(canvas, snapshot.page);
        break;
    case PageSafety:
        draw_safety_info(canvas, snapshot.page);
        break;
    default:
        draw_main(canvas, &snapshot);
        break;
    }
}

/* ---------------------------------------------------------- app logic */

static void servo_motor_input(InputEvent* event, void* context) {
    ServoMotorApp* app = context;
    furi_message_queue_put(app->event_queue, event, 0);
}

/* Moves the simulated horn toward the commanded angle. */
static void servo_anim_tick(void* context) {
    ServoMotorApp* app = context;
    bool changed = false;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    int16_t target = (int16_t)app->angle * 10;
    if(app->shown_x10 < target) {
        app->shown_x10 =
            (target - app->shown_x10 > ANIM_STEP_X10) ? app->shown_x10 + ANIM_STEP_X10 : target;
        changed = true;
    } else if(app->shown_x10 > target) {
        app->shown_x10 =
            (app->shown_x10 - target > ANIM_STEP_X10) ? app->shown_x10 - ANIM_STEP_X10 : target;
        changed = true;
    }
    bool on_main = app->page == PageMain;
    furi_mutex_release(app->mutex);

    if(changed && on_main) view_port_update(app->view_port);
}

static void servo_sweep_tick(void* context) {
    ServoMotorApp* app = context;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->running && app->mode == ServoModeSweep) {
        app->angle = app->angle == SERVO_ANGLE_MIN ? SERVO_ANGLE_MAX : SERVO_ANGLE_MIN;
        app->pulse_us = angle_to_pulse(app->angle);
        servo_pwm_set_pulse(app->pulse_us);
    }
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void servo_set_angle(ServoMotorApp* app, uint8_t angle) {
    if(angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->angle = angle;
    app->pulse_us = angle_to_pulse(angle);
    servo_pwm_set_pulse(app->pulse_us);
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void servo_set_mode(ServoMotorApp* app, ServoMode mode) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->mode = mode;
    furi_mutex_release(app->mutex);

    if(mode == ServoModeSweep) {
        servo_set_angle(app, SERVO_ANGLE_MIN);
        furi_timer_start(app->sweep_timer, furi_ms_to_ticks(SWEEP_DWELL_MS));
    } else {
        furi_timer_stop(app->sweep_timer);
        if(mode == ServoModeCenter)
            servo_set_angle(app, 90);
        else
            view_port_update(app->view_port);
    }
}

static void servo_set_page(ServoMotorApp* app, Page page) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->page = page;
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void servo_process_input(ServoMotorApp* app, InputEvent* event) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    Page page = app->page;
    ServoMode mode = app->mode;
    uint8_t angle = app->angle;
    furi_mutex_release(app->mutex);

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        if(page != PageMain) {
            servo_set_page(app, PageMain);
        } else {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->running = false;
            furi_mutex_release(app->mutex);
        }
        return;
    }

    /* Left/Right: 10 degree step on press, repeated while held. */
    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        if(page != PageMain) return;
        if(event->type != InputTypePress && event->type != InputTypeRepeat) return;

        /* Adjusting from Center or Sweep switches to Manual first. */
        if(mode != ServoModeManual) {
            servo_set_mode(app, ServoModeManual);
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            angle = app->angle;
            furi_mutex_release(app->mutex);
        }

        int dir = event->key == InputKeyRight ? 1 : -1;
        int next = angle + dir * SERVO_ANGLE_STEP;
        if(next < SERVO_ANGLE_MIN) next = SERVO_ANGLE_MIN;
        if(next > SERVO_ANGLE_MAX) next = SERVO_ANGLE_MAX;
        if(next != angle) servo_set_angle(app, (uint8_t)next);
        return;
    }

    if(event->type != InputTypePress) return;

    switch(event->key) {
    case InputKeyDown:
        servo_set_page(app, (Page)((page + 1) % PageCount));
        break;
    case InputKeyUp:
        servo_set_page(app, (Page)((page + PageCount - 1) % PageCount));
        break;
    case InputKeyOk:
        if(page != PageMain) {
            servo_set_page(app, PageMain);
        } else {
            servo_set_mode(app, (ServoMode)((mode + 1) % ServoModeCount));
        }
        break;
    default:
        break;
    }
}

static bool servo_enable_5v(ServoMotorApp* app) {
    app->power = furi_record_open(RECORD_POWER);
    app->otg_was_enabled = power_is_otg_enabled(app->power);

    PowerInfo power_info;
    power_get_info(app->power, &power_info);
    bool usb_powered = power_info.voltage_vbus > 4.0f;

    if(!app->otg_was_enabled && !usb_powered) {
        power_enable_otg(app->power, true);
        app->otg_enabled_by_app = power_is_otg_enabled(app->power);
    }

    app->five_volts_available = usb_powered || power_is_otg_enabled(app->power);
    return app->five_volts_available;
}

static void servo_restore_5v(ServoMotorApp* app) {
    if(app->otg_enabled_by_app && !app->otg_was_enabled) {
        power_enable_otg(app->power, false);
    }
    furi_record_close(RECORD_POWER);
}

int32_t servomotor_app(void* p) {
    UNUSED(p);
    ServoMotorApp app = {
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .event_queue = furi_message_queue_alloc(8, sizeof(InputEvent)),
        .pulse_us = angle_to_pulse(90),
        .angle = 90,
        .shown_x10 = 900,
        .mode = ServoModeManual,
        .page = PageMain,
        .running = true,
    };

    servo_enable_5v(&app);
    servo_pwm_start(app.pulse_us);

    app.sweep_timer = furi_timer_alloc(servo_sweep_tick, FuriTimerTypePeriodic, &app);
    app.anim_timer = furi_timer_alloc(servo_anim_tick, FuriTimerTypePeriodic, &app);

    app.view_port = view_port_alloc();
    view_port_draw_callback_set(app.view_port, servo_motor_draw, &app);
    view_port_input_callback_set(app.view_port, servo_motor_input, &app);
    app.gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);
    view_port_update(app.view_port);
    furi_timer_start(app.anim_timer, furi_ms_to_ticks(ANIM_TICK_MS));

    while(app.running) {
        InputEvent event;
        if(furi_message_queue_get(app.event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            servo_process_input(&app, &event);
        }
    }

    furi_timer_stop(app.anim_timer);
    furi_timer_free(app.anim_timer);
    furi_timer_stop(app.sweep_timer);
    furi_timer_free(app.sweep_timer);

    furi_mutex_acquire(app.mutex, FuriWaitForever);
    app.running = false;
    furi_mutex_release(app.mutex);
    servo_pwm_stop();

    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app.event_queue);
    servo_restore_5v(&app);
    furi_mutex_free(app.mutex);
    return 0;
}
