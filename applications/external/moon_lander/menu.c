#include "menu.h"
#include "lander_sprite.h"
#include <gui/canvas.h>
#include <stdio.h>

/* ----- Mode tables -------------------------------------------------------
 * Order here must match the enums in moon_lander.h. The button-only thrust
 * modes come first, easiest (Tap) first, then the Video Game Module tilt
 * modes; fuel modes go easiest-to-hardest top to bottom. */

const char* const thrust_mode_label[ThrustModeCount] = {
    "Button Tap Impulse",
    "Button Binary",
    "Button Ramp",
    "Vidya module Tilt+Tap",
    "Vidya module Tilt+Binary",
    "Vidya module Tilt+Ramp",
    "Vidya module Full Tilt",
};

const char* const thrust_key_label[ThrustKeyCount] = {
    "UP",
    "OK",
};

void thrust_mode_desc(ThrustMode mode, ThrustKey key, char* buf, size_t size) {
    const char* k = thrust_key_label[key];
    switch(mode) {
        case ThrustModeTapImpulse:  snprintf(buf, size, "Tap %s = one burst", k);           break;
        case ThrustModeBinary:      snprintf(buf, size, "Hold %s = full thrust", k);        break;
        case ThrustModeRamp:        snprintf(buf, size, "Hold %s, thrust ramps up", k);     break;
        case ThrustModeVidyaTap:    snprintf(buf, size, "Tilt steers, tap %s=burst", k);    break;
        case ThrustModeVidyaBinary: snprintf(buf, size, "Tilt steers, hold %s=thrust", k);  break;
        case ThrustModeVidyaRamp:   snprintf(buf, size, "Tilt steers, %s ramps thrust", k); break;
        default:                    snprintf(buf, size, "Tilt steers + fires thrusters");   break;
    }
}

const char* const difficulty_label[DifficultyCount] = {
    "Easy",
    "Medium",
    "Hard",
    "Realistic",
    "Custom",
};

const char* const sound_level_label[SoundCount] = {
    "Off", "Low", "Med", "High",
};

const char* const vibration_level_label[VibrationCount] = {
    "Off", "Low", "Med", "High",
};

const char* const fuel_mode_label[FuelModeCount] = {
    "Full fuel each lvl",
    "No refuel, Easy: 500",
    "No refuel, Med: 350",
    "No refuel, Hard: 200",
};

const char* const fuel_mode_desc[FuelModeCount] = {
    "Refills to 100 per lvl",
    "500 total, no top-ups",
    "350 total, no top-ups",
    "200 total, no top-ups",
};

const int fuel_mode_starting[FuelModeCount] = {
    100,
    500,
    350,
    200,
};

/* ----- Drawing ----------------------------------------------------------- */

/* 7x8 wrench icon, centered at (cx, cy).
 *   ###....    head: top bridge
 *   ..#....    head: right wall (C opens left — 2px gap = jaw)
 *   ..#....    head: right wall
 *   ###....    head: bottom bridge
 *   ..##...    handle (diagonal down-right)
 *   ...##..
 *   ....##.
 *   .....##
 */
static void draw_wrench_icon(Canvas* canvas, int cx, int cy) {
    /* Head: C-shape opening to the left (jaw for gripping) */
    canvas_draw_line(canvas, cx - 3, cy - 4, cx - 1, cy - 4);  // top
    canvas_draw_line(canvas,  cx - 1, cy - 3, cx - 1, cy - 2); // right wall
    canvas_draw_line(canvas,  cx, cy - 3, cx, cy - 2);         // double thick right wall
    canvas_draw_line(canvas, cx - 3, cy - 1, cx, cy - 1);  // bottom
    /* Handle: diagonal 2px-wide line */
    canvas_draw_line(canvas, cx,   cy,      cx+1,   cy);
    canvas_draw_line(canvas, cx+1,  cy + 1, cx + 2, cy + 1);
    canvas_draw_line(canvas, cx+2, cy + 2,  cx + 3, cy + 2);
    canvas_draw_line(canvas, cx+3, cy + 3,  cx + 4, cy + 3);
}

/* Title bar layout — three zones (22/84/22 px) across 128px:
 *   [ sprite  0..21 ] [ "MOON LANDER"  22..105 ] [ wrench  106..127 ]
 *   → score            → about                      → settings
 *   Icon centers: x=12, x=64, x=116 (52px apart each).
 *   Separator line at y=10; content lives in y=0..9. */
static void draw_title(Canvas* canvas, const MenuState* m) {
    bool score_focused    = (m->row == MenuRowTitle && m->title_sel == MenuTitleSelScore);
    bool about_focused    = (m->row == MenuRowTitle && m->title_sel == MenuTitleSelAbout);
    bool settings_focused = (m->row == MenuRowTitle && m->title_sel == MenuTitleSelSettings);

    /* Sprite button — opens high score screen */
    if(score_focused) {
        canvas_draw_rbox(canvas, 0, 0, 22, 10, 2);
        canvas_set_color(canvas, ColorWhite);
    }
    lander_draw_static(canvas, 12, 5);
    if(score_focused) canvas_set_color(canvas, ColorBlack);

    /* About button — title text */
    if(about_focused) {
        canvas_draw_rbox(canvas, 22, 0, 84, 10, 2);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignCenter, "MOON LANDER");
    if(about_focused) canvas_set_color(canvas, ColorBlack);

    /* Settings button */
    if(settings_focused) {
        canvas_draw_rbox(canvas, 106, 0, 22, 10, 2);
        canvas_set_color(canvas, ColorWhite);
    }
    draw_wrench_icon(canvas, 116, 5);
    if(settings_focused) canvas_set_color(canvas, ColorBlack);

    canvas_draw_line(canvas, 0, 10, SCREEN_W - 1, 10);
}

/* Selector row factored so thrust and fuel share the same chrome.
 * `label` is what shows in the middle; `focused` controls the inverted look. */
void draw_selector_row_w(Canvas* canvas, int x, int y, int w, const char* label, bool focused) {
    if (focused) {
        canvas_draw_rbox(canvas, x, y, w, 12, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, 12, 2);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x + 5, y + 6, AlignLeft, AlignCenter, "<");
    canvas_draw_str_aligned(canvas, x + w - 5, y + 6, AlignRight, AlignCenter, ">");
    canvas_draw_str_aligned(canvas, x + w / 2, y + 6, AlignCenter, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

void draw_selector_row(Canvas* canvas, int y, const char* label, bool focused) {
    draw_selector_row_w(canvas, 2, y, SCREEN_W - 4, label, focused);
}

/* Single description line shown beneath the selectors. Whichever row is
 * focused, its description shows. Title/Buttons rows → no description. */
static void draw_focused_desc(Canvas* canvas, const MenuState* m, int y) {
    char buf[32];
    const char* desc = NULL;
    if (m->row == MenuRowThrust) {
        thrust_mode_desc(m->thrust_mode, m->thrust_key, buf, sizeof(buf));
        desc = buf;
    } else if (m->row == MenuRowFuel) {
        desc = fuel_mode_desc[m->fuel_mode];
    }
    if (!desc) return;
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, y, AlignCenter, AlignTop, desc);
}

static void draw_button(
    Canvas* canvas, int x, int y, int w, int h, const char* label, bool selected) {
    if (selected) {
        canvas_draw_rbox(canvas, x, y, w, h, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, h, 2);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x + w / 2, y + h / 2, AlignCenter, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_button_row(Canvas* canvas, const MenuState* m, int y) {
    bool row_focused = (m->row == MenuRowButtons);
    const int btn_w = 56, btn_h = 12, gap = 4;
    int x0 = (SCREEN_W - (btn_w * 2 + gap)) / 2;
    draw_button(canvas, x0,                    y, btn_w, btn_h, "START",
                row_focused && m->btn == MenuBtnStart);
    draw_button(canvas, x0 + btn_w + gap,      y, btn_w, btn_h, "TUTORIAL",
                row_focused && m->btn == MenuBtnTutorial);
}

void menu_draw(Canvas* canvas, const MenuState* m) {
    draw_title(canvas, m);
    /* Layout:
     *   0..10   title bar + separator
     *   12..23  thrust selector
     *   25..36  fuel selector
     *   38..45  description for focused row (blank on Title/Buttons)
     *   52..63  Start / Tutorial buttons
     */
    draw_selector_row(canvas, 12, thrust_mode_label[m->thrust_mode], m->row == MenuRowThrust);
    draw_selector_row(canvas, 25, fuel_mode_label[m->fuel_mode],     m->row == MenuRowFuel);
    draw_focused_desc(canvas, m, 38);
    draw_button_row(canvas, m, 52);
}

/* ----- State & input ----------------------------------------------------- */

void menu_init(MenuState* m) {
    m->thrust_mode  = ThrustModeTapImpulse;
    m->fuel_mode    = FuelModeFull;
    m->difficulty   = DifficultyMedium;
    m->btn          = MenuBtnStart;
    m->row          = MenuRowThrust;
    m->title_sel    = MenuTitleSelAbout;
    m->sound_level     = SoundMed;
    m->vibration_level = VibrationHigh;
    m->custom_vx       = CUSTOM_VX_MAX;   // Custom starts out equal to Easy
    m->custom_vy       = CUSTOM_VY_MAX;
    m->custom_angle    = CUSTOM_ANGLE_MAX;
    m->tv_mode         = false;
    m->thrust_key      = ThrustKeyUp;
    m->seed            = SEED_MIN;
}

MenuAction menu_input(MenuState* m, const InputEvent* ev) {
    if (ev->type != InputTypeShort && ev->type != InputTypeRepeat) return MenuActionNone;

    switch (ev->key) {
        case InputKeyUp:
            if (m->row > 0) m->row--;
            break;
        case InputKeyDown:
            if (m->row < MenuRowCount - 1) m->row++;
            break;
        case InputKeyLeft:
            if (m->row == MenuRowTitle) {
                if (m->title_sel > 0) m->title_sel--;
            } else if (m->row == MenuRowThrust) {
                m->thrust_mode = (m->thrust_mode + ThrustModeCount - 1) % ThrustModeCount;
            } else if (m->row == MenuRowFuel) {
                m->fuel_mode = (m->fuel_mode + FuelModeCount - 1) % FuelModeCount;
            } else if (m->row == MenuRowButtons && m->btn > 0) {
                m->btn--;
            }
            break;
        case InputKeyRight:
            if (m->row == MenuRowTitle) {
                if (m->title_sel < MenuTitleSelCount - 1) m->title_sel++;
            } else if (m->row == MenuRowThrust) {
                m->thrust_mode = (m->thrust_mode + 1) % ThrustModeCount;
            } else if (m->row == MenuRowFuel) {
                m->fuel_mode = (m->fuel_mode + 1) % FuelModeCount;
            } else if (m->row == MenuRowButtons && m->btn < MenuBtnCount - 1) {
                m->btn++;
            }
            break;
        case InputKeyOk:
            if (m->row == MenuRowTitle) {
                if(m->title_sel == MenuTitleSelScore)    return MenuActionScore;
                if(m->title_sel == MenuTitleSelAbout)    return MenuActionInfo;
                return MenuActionSettings;
            } else if (m->row == MenuRowButtons) {
                return (m->btn == MenuBtnStart) ? MenuActionStart : MenuActionTutorial;
            }
            break;
        case InputKeyBack:
            /* Tap only: a Back still held from leaving a game (a 1 s hold)
             * keeps sending repeats, which must not close the app. */
            if (ev->type == InputTypeShort) return MenuActionExit;
            break;
        default:
            break;
    }
    return MenuActionNone;
}
