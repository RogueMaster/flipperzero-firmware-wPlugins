#include "ui.h"
#include "app_settings.h"
#include "nus_transcript.h"
#include <gui/elements.h>
#include <string.h>
#include <stdio.h>

// Default slash commands (shown until bridge sends an updated list)
static const char* default_menu_items[] = {
    "/add-dir",
    "/agents",
    "/autofix-pr",
    "/batch",
    "/branch",
    "/btw",
    "/buddy",
    "/chrome",
    "/claude-api",
    "/clear",
    "/color",
    "/compact",
    "/config",
    "/context",
    "/copy",
    "/debug",
    "/desktop",
    "/diff",
    "/doctor",
    "/effort",
    "/exit",
    "/export",
    "/extra-usage",
    "/fast",
    "/feedback",
    "/help",
    "/hooks",
    "/ide",
    "/init",
    "/insights",
    "/install-github-app",
    "/install-slack-app",
    "/keybindings",
    "/login",
    "/logout",
    "/loop",
    "/mcp",
    "/memory",
    "/mobile",
    "/model",
    "/permissions",
    "/plan",
    "/plugin",
    "/powerup",
    "/release-notes",
    "/reload-plugins",
    "/remote-control",
    "/remote-env",
    "/rename",
    "/resume",
    "/review",
    "/rewind",
    "/sandbox",
    "/schedule",
    "/security-review",
    "/skills",
    "/stats",
    "/status",
    "/statusline",
    "/stickers",
    "/tasks",
    "/team-onboarding",
    "/teleport",
    "/terminal-setup",
    "/theme",
    "/ultraplan",
    "/update-config",
    "/usage",
    "/voice",
};
#define DEFAULT_MENU_COUNT 69

// ── Layout ───────────────────────────────────────────────────────

#define HDR_H  9   // inverted header bar height (y 0..8)
#define FTR_Y  53  // footer separator y

// ── Animation Timer ──────────────────────────────────────────────

static void anim_tick(void* context) {
    UiState* ui = context;
    StatusModel* sm = view_get_model(ui->status_view);
    sm->anim_frame++;
    // Auto-reset transient poses after ~3s (20 frames * 150ms)
    if((sm->pose == PoseHappy || sm->pose == PoseAlert || sm->pose == PoseExcited) &&
       sm->anim_frame > 20) {
        sm->pose = PoseIdle;
        sm->anim_frame = 0;
    }
    view_commit_model(ui->status_view, true);

    PermModel* pm = view_get_model(ui->perm_view);
    pm->anim_frame++;
    view_commit_model(ui->perm_view, true);

    InfoModel* im = view_get_model(ui->info_view);
    im->anim_frame++;
    view_commit_model(ui->info_view, true);
}

// ── Button Hints ─────────────────────────────────────────────────

// ◄ triangle + label, bottom-left
static void hint_left(Canvas* canvas, const char* label) {
    canvas_set_font(canvas, FontSecondary);
    const int y = 59;
    canvas_draw_line(canvas, 0, y, 6, y);
    canvas_draw_line(canvas, 2, y - 1, 6, y - 1);
    canvas_draw_line(canvas, 2, y + 1, 6, y + 1);
    canvas_draw_line(canvas, 4, y - 2, 6, y - 2);
    canvas_draw_line(canvas, 4, y + 2, 6, y + 2);
    canvas_draw_dot(canvas, 6, y - 3);
    canvas_draw_dot(canvas, 6, y + 3);
    canvas_draw_str_aligned(canvas, 9, y + 1, AlignLeft, AlignCenter, label);
}

// ► triangle + label, bottom-right
static void hint_right(Canvas* canvas, const char* label) {
    canvas_set_font(canvas, FontSecondary);
    int lw = (int)canvas_string_width(canvas, label);
    const int y = 59;
    int bx = 127 - lw - 2 - 7;
    canvas_draw_line(canvas, bx, y, bx + 6, y);
    canvas_draw_line(canvas, bx, y - 1, bx + 4, y - 1);
    canvas_draw_line(canvas, bx, y + 1, bx + 4, y + 1);
    canvas_draw_line(canvas, bx, y - 2, bx + 2, y - 2);
    canvas_draw_line(canvas, bx, y + 2, bx + 2, y + 2);
    canvas_draw_dot(canvas, bx, y - 3);
    canvas_draw_dot(canvas, bx, y + 3);
    canvas_draw_str(canvas, 127 - lw, 63, label);
}

// ● circle + label, bottom-center
static void hint_ok(Canvas* canvas, const char* label) {
    canvas_set_font(canvas, FontSecondary);
    int lw = (int)canvas_string_width(canvas, label);
    int total = 6 + 2 + lw;
    int ix = 64 - total / 2 + 3;
    const int y = 59;
    canvas_draw_circle(canvas, ix, y, 4);
    canvas_draw_disc(canvas, ix, y, 2);
    canvas_draw_str(canvas, ix + 6, 63, label);
}

// ↩ arrow + label, bottom-right
static void hint_back(Canvas* canvas, const char* label) {
    canvas_set_font(canvas, FontSecondary);
    int lw = (int)canvas_string_width(canvas, label);
    int ax = 127 - lw - 11;
    int ay = 57;
    canvas_draw_line(canvas, ax + 3, ay + 4, ax + 7, ay + 4);
    canvas_draw_line(canvas, ax + 7, ay, ax + 7, ay + 4);
    canvas_draw_line(canvas, ax, ay, ax + 7, ay);
    canvas_draw_line(canvas, ax, ay, ax + 2, ay - 2);
    canvas_draw_line(canvas, ax, ay, ax + 2, ay + 2);
    canvas_draw_str(canvas, 127 - lw, 63, label);
}

// ── Shared Drawing ───────────────────────────────────────────────

// Header bar with bottom separator (dark = inverted white-on-black)
static void draw_header(Canvas* canvas, const char* title, bool dark) {
    if(dark) {
        canvas_draw_box(canvas, 0, 0, 128, HDR_H);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 64, HDR_H / 2 + 1, AlignCenter, AlignCenter, title);
    if(dark) {
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_line(canvas, 0, HDR_H, 127, HDR_H);
    }
}

// Footer separator line
static void draw_footer_sep(Canvas* canvas) {
    canvas_draw_line(canvas, 0, FTR_Y, 127, FTR_Y);
}

// Vertical scrollbar on right edge
static void draw_scrollbar(Canvas* canvas, int idx, int count, int y0, int y1) {
    if(count <= 1) return;
    int h = y1 - y0;
    int bh = h / count;
    if(bh < 4) bh = 4;
    int by = y0 + (idx * (h - bh)) / (count - 1);
    canvas_draw_line(canvas, 126, y0, 126, y1); // track
    canvas_draw_box(canvas, 125, by, 3, bh);    // thumb
}

// Button icon IDs (used by help page and status hint)
enum {
    HelpBtnUp,
    HelpBtnLeft,
    HelpBtnRight,
    HelpBtnOk,
    HelpBtnDown,
    HelpBtnBack,
    HelpBtnText, // sentinel: render desc as plain text (no icon, Hold, or colon)
};
static void draw_help_icon(Canvas* canvas, int x, int y, uint8_t button);

// Claude character (original Claude Code icon design + animated poses)
// Body: 18x10 rectangle, ears: dots, legs: 4 lines, eyes: 2x3 white cutouts
// Small + sparkle at (x, y)
static void draw_sparkle(Canvas* canvas, int x, int y) {
    canvas_draw_dot(canvas, x, y);
    canvas_draw_dot(canvas, x - 1, y);
    canvas_draw_dot(canvas, x + 1, y);
    canvas_draw_dot(canvas, x, y - 1);
    canvas_draw_dot(canvas, x, y + 1);
}

static uint8_t rssi_to_bars(int rssi) {
    if(rssi > -65) return 4;
    if(rssi > -75) return 3;
    if(rssi > -85) return 2;
    if(rssi > -90) return 1;
    return 0;
}

static void draw_claude(Canvas* canvas, int cx, int cy, uint8_t pose, uint8_t frame) {
    // ── Pose-specific body offsets ──
    if(pose == PoseHappy) {
        // Gentle bounce: up for first 4 frames
        if(frame < 2) cy -= 2;
        else if(frame < 4) cy -= 1;
    }
    if(pose == PoseExcited) {
        // Fast bounce: repeating 4-frame cycle
        int ph = frame % 4;
        if(ph == 0) cy -= 3;
        else if(ph <= 2) cy -= 1;
    }
    if(pose == PoseAlert) {
        // Horizontal shake: ±1px alternating every 2 frames
        cx += (frame % 4 < 2) ? 1 : -1;
    }
    if(pose == PoseWorried) {
        // Gentle horizontal wobble: ±1px every 3 frames
        cx += (frame % 6 < 3) ? 1 : -1;
    }

    canvas_set_color(canvas, ColorBlack);

    // Body (18x10 rectangle - the Claude Code icon)
    canvas_draw_box(canvas, cx, cy, 18, 10);

    // Ears (single dots on each side)
    canvas_draw_dot(canvas, cx - 1, cy + 4);
    canvas_draw_dot(canvas, cx + 18, cy + 4);

    // Legs (4 lines of 2px, 1px gap below body)
    canvas_draw_line(canvas, cx + 4, cy + 11, cx + 4, cy + 12);
    canvas_draw_line(canvas, cx + 6, cy + 11, cx + 6, cy + 12);
    canvas_draw_line(canvas, cx + 12, cy + 11, cx + 12, cy + 12);
    canvas_draw_line(canvas, cx + 14, cy + 11, cx + 14, cy + 12);

    // ── Eyes (white cutouts, vary by pose & frame) ──
    canvas_set_color(canvas, ColorWhite);

    switch(pose) {
    case PoseIdle:
    default:
        if(frame % 20 >= 18) {
            // Blink: thin horizontal lines
            canvas_draw_line(canvas, cx + 4, cy + 4, cx + 5, cy + 4);
            canvas_draw_line(canvas, cx + 12, cy + 4, cx + 13, cy + 4);
        } else {
            // Normal eyes
            canvas_draw_box(canvas, cx + 4, cy + 3, 2, 3);
            canvas_draw_box(canvas, cx + 12, cy + 3, 2, 3);
        }
        break;

    case PoseListening:
        // Eyes shifted up (looking toward mic)
        canvas_draw_box(canvas, cx + 4, cy + 2, 2, 3);
        canvas_draw_box(canvas, cx + 12, cy + 2, 2, 3);
        break;

    case PoseThinking:
        // Eyes shifted right (looking at status text)
        canvas_draw_box(canvas, cx + 5, cy + 3, 2, 3);
        canvas_draw_box(canvas, cx + 13, cy + 3, 2, 3);
        break;

    case PoseHappy:
    case PoseExcited:
        // Squinted happy eyes (shorter) + smile
        canvas_draw_box(canvas, cx + 4, cy + 3, 2, 2);
        canvas_draw_box(canvas, cx + 12, cy + 3, 2, 2);
        canvas_draw_dot(canvas, cx + 7, cy + 7);
        canvas_draw_dot(canvas, cx + 10, cy + 7);
        canvas_draw_line(canvas, cx + 8, cy + 8, cx + 9, cy + 8);
        break;

    case PoseAlert:
        // Wide eyes (taller)
        canvas_draw_box(canvas, cx + 4, cy + 2, 2, 4);
        canvas_draw_box(canvas, cx + 12, cy + 2, 2, 4);
        break;

    case PoseSleeping:
        // Closed eyes (horizontal lines)
        canvas_draw_line(canvas, cx + 4, cy + 4, cx + 5, cy + 4);
        canvas_draw_line(canvas, cx + 12, cy + 4, cx + 13, cy + 4);
        break;

    case PoseWorried: {
        // Shifty eyes: cycle left → center → right every 5 frames
        int shift = ((frame / 5) % 3) - 1; // -1, 0, +1
        canvas_draw_box(canvas, cx + 4 + shift, cy + 3, 2, 3);
        canvas_draw_box(canvas, cx + 12 + shift, cy + 3, 2, 3);
        break;
    }
    }

    canvas_set_color(canvas, ColorBlack);

    // ── Extra animations outside the body ──
    switch(pose) {
    case PoseThinking: {
        // Animated thinking dots above character
        int phase = (frame / 4) % 4;
        if(phase >= 1) canvas_draw_dot(canvas, cx + 10, cy - 3);
        if(phase >= 2) canvas_draw_dot(canvas, cx + 13, cy - 3);
        if(phase >= 3) canvas_draw_dot(canvas, cx + 16, cy - 3);
        break;
    }
    case PoseAlert: {
        // Blinking exclamation mark above character
        if(frame % 6 < 3) {
            canvas_draw_line(canvas, cx + 8, cy - 5, cx + 8, cy - 3);
            canvas_draw_dot(canvas, cx + 8, cy - 1);
        }
        break;
    }
    case PoseSleeping: {
        // Floating "z" above character
        canvas_set_font(canvas, FontSecondary);
        int zy = cy - 2 - ((frame % 10) / 2);
        if(zy > HDR_H) {
            canvas_draw_str(canvas, cx + 16, zy, "z");
        }
        break;
    }
    case PoseHappy: {
        // Sparkle stars appear on each side after the bounce settles
        if(frame >= 4) {
            if(frame % 5 < 4)
                draw_sparkle(canvas, cx - 3, cy + 5);
            if((frame + 2) % 5 < 4)
                draw_sparkle(canvas, cx + 21, cy + 5);
        }
        break;
    }
    case PoseExcited: {
        // Arms raised (lines from body sides pointing up-outward)
        canvas_draw_line(canvas, cx - 1, cy + 3, cx - 3, cy + 1);
        canvas_draw_line(canvas, cx + 18, cy + 3, cx + 20, cy + 1);
        // 4 sparkles cycling in with staggered phases
        const int8_t sp_dx[] = {-4, 21, 5, 14};
        const int8_t sp_dy[] = {4, 4, -5, -5};
        for(int i = 0; i < 4; i++) {
            if((frame + i * 3) % 8 < 6)
                draw_sparkle(canvas, cx + sp_dx[i], cy + sp_dy[i]);
        }
        break;
    }
    case PoseWorried: {
        // Sweat drop (upper-right of body, animated drip)
        int drop_y = cy - 1 + (frame % 4) / 2; // slowly slides down
        canvas_draw_dot(canvas, cx + 17, drop_y);
        canvas_draw_dot(canvas, cx + 16, drop_y + 1);
        canvas_draw_dot(canvas, cx + 18, drop_y + 1);
        canvas_draw_dot(canvas, cx + 17, drop_y + 2);
        break;
    }
    default:
        break;
    }
}

// ── Status View ──────────────────────────────────────────────────

// Greedy word-wrap. Fills `lines` (up to `max_lines` × `line_cap`), returns
// the line count. Breaks at spaces where possible, otherwise mid-word.
static int wrap_text(
    Canvas* canvas,
    const char* text,
    int max_width,
    char (*lines)[32],
    int max_lines) {
    if(!canvas || !text || !*text || max_lines <= 0) return 0;
    int nlines = 0;
    const char* p = text;
    char buf[32];
    while(*p && nlines < max_lines) {
        const char* start = p;
        int last_space = -1;      // index (relative to start) of last fit-so-far space
        int fit = 0;              // chars that fit on this line
        int newline_at = -1;      // index of forced break, if any
        int i = 0;
        while(start[i]) {
            if(start[i] == '\n') { newline_at = i; break; }
            int len = i + 1;
            if(len >= (int)sizeof(buf)) break;
            memcpy(buf, start, len);
            buf[len] = '\0';
            if((int)canvas_string_width(canvas, buf) > max_width) break;
            if(start[i] == ' ') last_space = i;
            fit = len;
            i++;
        }
        int cut;
        bool skip_newline = false;
        if(newline_at >= 0) {
            cut = newline_at;
            skip_newline = true;
        } else if(!start[fit]) {
            cut = fit; // end of string fits
        } else if(last_space > 0 && nlines + 1 < max_lines) {
            cut = last_space; // break at space (keeps future lines available)
        } else {
            cut = (fit > 0) ? fit : 1; // mid-word break (last line or single long word)
        }
        if(cut > (int)sizeof(lines[0]) - 1) cut = (int)sizeof(lines[0]) - 1;
        memcpy(lines[nlines], start, cut);
        lines[nlines][cut] = '\0';
        nlines++;
        p = start + cut;
        if(skip_newline && *p == '\n') p++;
        while(*p == ' ') p++;
    }
    return nlines;
}

static void status_draw(Canvas* canvas, void* model) {
    if(!canvas || !model) return;
    StatusModel* m = model;
    canvas_clear(canvas);

    bool desktop = app_settings_get_ble_mode() == BleModeDesktop;

    // ── Header bar (light theme with bottom separator) ──
    canvas_set_font(canvas, FontSecondary);

    if(m->pose == PoseListening || m->space_hold_active) {
        // Pulsing recording indicator centered in header (Bridge mode only —
        // voice dictation isn't available in Desktop mode).
        if(!desktop) {
            int tw = (int)canvas_string_width(canvas, "REC");
            int total_w = 5 + 3 + tw;
            int ox = 64 - total_w / 2;
            if(m->anim_frame % 6 < 3)
                canvas_draw_disc(canvas, ox + 2, 5, 2);
            else
                canvas_draw_circle(canvas, ox + 2, 5, 2);
            canvas_draw_str(canvas, ox + 8, 8, "REC");
        }
    } else if(!desktop) {
        // Centered ▲ Mic hint (Bridge mode only)
        int tw = (int)canvas_string_width(canvas, "Mic");
        int total_w = 5 + 3 + tw;
        int ox = 64 - total_w / 2;
        canvas_draw_dot(canvas, ox + 2, 3);
        canvas_draw_line(canvas, ox + 1, 4, ox + 3, 4);
        canvas_draw_line(canvas, ox, 5, ox + 4, 5);
        canvas_draw_str(canvas, ox + 8, 8, "Mic");
    }

    // Mute indicator — small 'M' at top-left when sound is off
    if(m->muted) {
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 1, 8, "M");
    }

    // Transport mode — only when connected
    if(m->connected) {
        if(m->transport_mode) {
            /* Signal bars are only meaningful in Bridge mode — the Python
             * host bridge reports RSSI in its pings.  In Desktop mode the
             * Anthropic protocol has no such field, so the bars would
             * just show a fixed value (always 0 or stale).  Hide them. */
            if(app_settings_get_ble_mode() != BleModeDesktop) {
                // BLE: draw signal bars (4 bars, increasing height, at right of header)
                // Bar positions x: 113, 115, 117, 119 — just left of the claude dot (124)
                const int bx = 113;
                const int by = 6; // bottom y of all bars
                const int heights[4] = {2, 3, 4, 5};
                for(int i = 0; i < 4; i++) {
                    int top = by - heights[i] + 1;
                    if(i < (int)m->rssi_bars) {
                        canvas_draw_line(canvas, bx + i * 2, top, bx + i * 2, by);
                    } else {
                        // Empty bar: just top and bottom dots
                        canvas_draw_dot(canvas, bx + i * 2, top);
                        canvas_draw_dot(canvas, bx + i * 2, by);
                    }
                }
            }
        } else {
            canvas_set_font(canvas, FontKeyboard);
            canvas_draw_str_aligned(
                canvas,
                119, HDR_H / 2 + 1,
                AlignRight, AlignCenter,
                "USB");
            canvas_set_font(canvas, FontSecondary);
        }
    }

    // Connectivity dot (Claude session) — right edge
    if(m->claude_connected)
        canvas_draw_disc(canvas, 124, 4, 2);
    else
        canvas_draw_circle(canvas, 124, 4, 2);

    // Header bottom separator
    canvas_draw_line(canvas, 0, HDR_H, 127, HDR_H);

    // ── Content area ──
    // Claude character (left, vertically centered)
    draw_claude(canvas, 4, 22, m->pose, m->anim_frame);

    // Status text (right of character)
    const char* main_text = m->connected ? "Ready" : "No connection";
    if(m->text[0] != '\0') main_text = m->text;

    if(desktop) {
        // Desktop mode has no subtext — wrap the main text across up to 4 lines.
        canvas_set_font(canvas, FontPrimary);
        char lines[4][32];
        int nlines = wrap_text(canvas, main_text, 97, lines, 4);
        if(nlines == 0) {
            nlines = 1;
            strncpy(lines[0], main_text, sizeof(lines[0]) - 1);
            lines[0][sizeof(lines[0]) - 1] = '\0';
        }
        const int line_h = 10;
        int total_h = nlines * line_h;
        int y0 = (HDR_H + FTR_Y) / 2 - total_h / 2 + line_h / 2;
        /* Left-align against the text column (x=30 starts just right of
         * the Claude character at x=4..~22, matches wrap_text's 97 px
         * width budget → right edge ≈ 127). */
        for(int i = 0; i < nlines; i++) {
            canvas_draw_str_aligned(
                canvas, 30, y0 + i * line_h, AlignLeft, AlignCenter, lines[i]);
        }
    } else {
        bool has_sub = m->subtext[0] != '\0';
        bool show_hint = !has_sub && m->connected && m->text[0] == '\0';

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 77, (has_sub || show_hint) ? 25 : 31, AlignCenter, AlignCenter, main_text);
        if(has_sub) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 77, 37, AlignCenter, AlignCenter, m->subtext);
        } else if(show_hint) {
            canvas_set_font(canvas, FontSecondary);
            // "Hold [►] for menu" with inline icon
            int hw = (int)canvas_string_width(canvas, "Hold ");
            int fw = (int)canvas_string_width(canvas, " for menu");
            int sw = (int)canvas_string_width(canvas, " "); // space after icon
            int total = hw + 5 + sw + fw; // 5px icon width + space
            int hx = 77 - total / 2;
            canvas_draw_str(canvas, hx, 39, "Hold ");
            draw_help_icon(canvas, hx + hw, 39, HelpBtnRight);
            canvas_draw_str(canvas, hx + hw + 6 + sw, 39, "for menu");
        }
    }

    // ── Footer ──
    draw_footer_sep(canvas);
    if(desktop) {
        /* Desktop (NUS) mode: key input isn't available, so Mic/Esc/Enter
         * are hidden. Both short- and long-press Right open the info menu. */
        hint_right(canvas, "Menu");
    } else {
        hint_left(canvas, "Esc");
        hint_ok(canvas, "Enter");
        hint_right(canvas, "Cmds");
    }
}

static bool status_input(InputEvent* event, void* context) {
    if(!event || !context) return false;
    UiState* ui = context;
    if(
        event->type != InputTypeShort && event->type != InputTypeLong &&
        event->type != InputTypeRelease)
        return false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventEnter, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyOk && event->type == InputTypeLong) {
        if(ui->event_callback) ui->event_callback(UiEventYes, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventEsc, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyLeft && event->type == InputTypeLong) {
        if(ui->event_callback) ui->event_callback(UiEventInterrupt, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyUp && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventVoice, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyUp && event->type == InputTypeLong) {
        ui->up_hold_active = true;
        StatusModel* m = view_get_model(ui->status_view);
        m->space_hold_active = true;
        view_commit_model(ui->status_view, true);
        if(ui->event_callback) {
            ui->event_callback(UiEventSpaceHoldStart, NULL, ui->event_context);
        }
        return true;
    }
    if(event->key == InputKeyUp && event->type == InputTypeRelease && ui->up_hold_active) {
        ui->up_hold_active = false;
        StatusModel* m = view_get_model(ui->status_view);
        m->space_hold_active = false;
        view_commit_model(ui->status_view, true);
        if(ui->event_callback) {
            ui->event_callback(UiEventSpaceHoldEnd, NULL, ui->event_context);
        }
        return true;
    }
    if(event->key == InputKeyDown && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventDown, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyDown && event->type == InputTypeLong) {
        if(ui->event_callback) ui->event_callback(UiEventToggleMute, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyRight && event->type == InputTypeShort) {
        /* Desktop mode: commands menu is useless (no keystrokes), so short-
         * press goes to the info Menu — same as long-press. */
        UiEventType evt = (app_settings_get_ble_mode() == BleModeDesktop)
                              ? UiEventOpenInfo
                              : UiEventOpenMenu;
        if(ui->event_callback) ui->event_callback(evt, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyRight && event->type == InputTypeLong) {
        if(ui->event_callback) ui->event_callback(UiEventOpenInfo, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventBackspace, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        if(ui->event_callback) ui->event_callback(UiEventExitApp, NULL, ui->event_context);
        return true;
    }
    return false;
}

// ── Menu View ────────────────────────────────────────────────────

static void menu_draw(Canvas* canvas, void* model) {
    if(!canvas || !model) return;
    MenuModel* m = model;
    canvas_clear(canvas);

    // ── Header ──
    draw_header(canvas, "COMMANDS", false);

    if(m->count == 0) {
        // Empty state with bordered message
        canvas_draw_rframe(canvas, 10, 18, 108, 28, 4);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "No commands yet");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Start Claude Code");
    } else {
        const int visible = 5;
        const int item_h = 8;
        const int list_y = 12;

        // Scroll window so selected item is always visible
        int start = 0;
        if(m->index >= visible) start = m->index - visible + 1;

        for(int vi = 0; vi < visible && start + vi < m->count; vi++) {
            int i = start + vi;
            int by = list_y + vi * item_h;

            canvas_set_font(canvas, FontSecondary);

            if(i == m->index) {
                // Selected: rounded inverted highlight, shifted upward
                // so it sits symmetrically around the glyph.
                canvas_draw_rbox(canvas, 1, by - 2, 121, item_h + 1, 1);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, 5, by + 6, m->items[i]);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, 5, by + 6, m->items[i]);
            }
        }

        // Scrollbar
        draw_scrollbar(canvas, m->index, m->count, list_y, list_y + visible * item_h);
    }

    // ── Footer ──
    draw_footer_sep(canvas);
    hint_ok(canvas, "Send");
    hint_back(canvas, "Back");
}

static bool menu_input(InputEvent* event, void* context) {
    if(!event || !context) return false;
    UiState* ui = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    MenuModel* m = view_get_model(ui->menu_view);

    if(event->key == InputKeyUp) {
        if(m->count > 0) m->index = (m->index > 0) ? m->index - 1 : m->count - 1;
        view_commit_model(ui->menu_view, true);
        return true;
    }
    if(event->key == InputKeyDown) {
        if(m->count > 0) m->index = (m->index < m->count - 1) ? m->index + 1 : 0;
        view_commit_model(ui->menu_view, true);
        return true;
    }
    if(event->key == InputKeyOk) {
        if(ui->event_callback && m->count > 0) {
            ui->event_callback(
                UiEventMenuSelect, m->items[m->index], ui->event_context);
            // Promote selected item to top for convenient re-use
            if(m->index > 0) {
                char tmp[MAX_MENU_ITEM_LEN];
                strncpy(tmp, m->items[m->index], MAX_MENU_ITEM_LEN - 1);
                tmp[MAX_MENU_ITEM_LEN - 1] = '\0';
                for(int i = m->index; i > 0; i--) {
                    strncpy(m->items[i], m->items[i - 1], MAX_MENU_ITEM_LEN);
                }
                strncpy(m->items[0], tmp, MAX_MENU_ITEM_LEN);
                m->index = 0;
                view_commit_model(ui->menu_view, true);
            }
        }
        return true;
    }
    if(event->key == InputKeyBack) {
        if(ui->event_callback) ui->event_callback(UiEventMenuBack, NULL, ui->event_context);
        return true;
    }
    return false;
}

// ── Info View ────────────────────────────────────────────────────

#define INFO_MENU_COUNT 5
/* Order: BLE mode toggle on top, Help just before About.  The BLE row
 * is rendered dynamically — label shows the *current* mode so user
 * sees what's active and toggles to the other. */
#define INFO_IDX_BLE      0
#define INFO_IDX_TRANS    1
#define INFO_IDX_SHIFTTAB 2
#define INFO_IDX_HELP     3
#define INFO_IDX_ABOUT    4
static const char* info_menu_items[INFO_MENU_COUNT] = {
    NULL /* BLE mode */, "Transcript", "Shift+Tab", "Help", "About"};

/* Shift+Tab is a Bridge-only action (the Flipper asks the host to send a
 * Shift+Tab keystroke into the active shell); in Desktop mode there is no
 * keystroke path, so hide the entry entirely. */
static bool info_menu_item_visible(int idx) {
    if(idx == INFO_IDX_SHIFTTAB && app_settings_get_ble_mode() == BleModeDesktop)
        return false;
    return true;
}

static int info_menu_step(int from, int delta) {
    /* Walk `delta` (±1) visible slots from `from`, wrapping within the
     * full menu; skip hidden items.  Returns `from` if no item is
     * visible (shouldn't happen — BLE/Help/About are always shown). */
    int i = from;
    for(int n = 0; n < INFO_MENU_COUNT; n++) {
        if(delta > 0)
            i = (i < INFO_MENU_COUNT - 1) ? i + 1 : 0;
        else
            i = (i > 0) ? i - 1 : INFO_MENU_COUNT - 1;
        if(info_menu_item_visible(i)) return i;
    }
    return from;
}

static const char* about_lines[] = {
    "Claude Buddy",
    "v0.6",
    "Claude Code companion",
    "by jxw1102",
    "github.com/jxw1102",
    "/flipper-claude-buddy",
};
#define ABOUT_LINE_COUNT 6
#define ABOUT_VISIBLE    3

typedef struct {
    uint8_t button;
    bool hold;
    const char* desc;
} HelpEntry;

/* Bridge mode: most buttons forward keystrokes to the host terminal via
 * the Python bridge.  Up is voice dictation, OK sends Enter, Left sends
 * Esc, etc.  HelpBtnText entries render as plain-text lines (no icon)
 * so we can mix a title and install instructions with the button rows. */
static const HelpEntry help_entries_bridge[] = {
    {HelpBtnText,  false, "Button actions:"},
    {HelpBtnUp,    false, "Voice dictation"},
    {HelpBtnUp,    true,  "Hold-to-talk"},
    {HelpBtnLeft,  false, "Interrupt (Esc)"},
    {HelpBtnLeft,  true,  "Ctrl+C"},
    {HelpBtnRight, false, "Cmd menu"},
    {HelpBtnRight, true,  "Menu"},
    {HelpBtnOk,    false, "Enter"},
    {HelpBtnOk,    true,  "Yes + Enter"},
    {HelpBtnDown,  false, "Down arrow"},
    {HelpBtnDown,  true,  "Mute"},
    {HelpBtnBack,  false, "Backspace"},
    {HelpBtnBack,  true,  "Exit app"},
    {HelpBtnText,  false, ""},
    {HelpBtnText,  false, "Install plugin:"},
    {HelpBtnText,  false, "```"},
    {HelpBtnText,  false, "claude plugin marketplace"},
    {HelpBtnText,  false, "add jxw1102/flipper-"},
    {HelpBtnText,  false, "claude-buddy"},
    {HelpBtnText,  false, ""},
    {HelpBtnText,  false, "claude plugin install"},
    {HelpBtnText,  false, "flipper-claude-buddy@"},
    {HelpBtnText,  false, "flipper-claude-buddy"},
    {HelpBtnText,  false, "```"},
};

/* Desktop (NUS) mode: no keystroke path, so a button-mapping list is
 * misleading.  Show a short onboarding/how-to instead.  Lines are sized
 * to use the full content width (~123 px, ~22 chars at FontSecondary). */
static const char* help_text_desktop[] = {
    "How to connect:",
    "",
    "In Claude Desktop app:",
    "1. Enable Developer Mode",
    "   (Help > Troubleshoot).",
    "2. Open Hardware Buddy",
    "   from Developer menu.",
    "3. Pick Flipper to pair.",
    "",
    "Hold Back to quit app.",
};
#define HELP_DESKTOP_LINES ((int)(sizeof(help_text_desktop) / sizeof(help_text_desktop[0])))
#define HELP_BRIDGE_LINES ((int)(sizeof(help_entries_bridge) / sizeof(help_entries_bridge[0])))

#define HELP_VISIBLE    4
#define HELP_LINE_H     10
#define HELP_COLON_X    31
#define HELP_DESC_X     36

// Small inline button icon at (x, baseline_y) — vertically centered with text
static void draw_help_icon(Canvas* canvas, int x, int y, uint8_t button) {
    int cy = y - 3;
    switch(button) {
    case HelpBtnUp: // ▲ (5w x 3h)
        canvas_draw_dot(canvas, x + 2, cy - 1);
        canvas_draw_line(canvas, x + 1, cy, x + 3, cy);
        canvas_draw_line(canvas, x, cy + 1, x + 4, cy + 1);
        break;
    case HelpBtnDown: // ▼ (5w x 3h)
        canvas_draw_line(canvas, x, cy - 1, x + 4, cy - 1);
        canvas_draw_line(canvas, x + 1, cy, x + 3, cy);
        canvas_draw_dot(canvas, x + 2, cy + 1);
        break;
    case HelpBtnLeft: // ◄ (5w x 5h)
        canvas_draw_dot(canvas, x, cy);
        canvas_draw_line(canvas, x + 1, cy - 1, x + 1, cy + 1);
        canvas_draw_line(canvas, x + 2, cy - 2, x + 2, cy + 2);
        canvas_draw_line(canvas, x + 3, cy - 2, x + 3, cy + 2);
        canvas_draw_dot(canvas, x + 4, cy - 2);
        canvas_draw_dot(canvas, x + 4, cy + 2);
        break;
    case HelpBtnRight: // ► (5w x 5h)
        canvas_draw_dot(canvas, x, cy - 2);
        canvas_draw_dot(canvas, x, cy + 2);
        canvas_draw_line(canvas, x + 1, cy - 2, x + 1, cy + 2);
        canvas_draw_line(canvas, x + 2, cy - 1, x + 2, cy + 1);
        canvas_draw_line(canvas, x + 3, cy - 1, x + 3, cy + 1);
        canvas_draw_dot(canvas, x + 4, cy);
        break;
    case HelpBtnOk: // ● (5w x 5h)
        canvas_draw_disc(canvas, x + 2, cy, 2);
        break;
    case HelpBtnBack: // ↩ (7w x 5h) — matches footer back icon
        canvas_draw_line(canvas, x, cy - 1, x + 5, cy - 1); // top bar
        canvas_draw_line(canvas, x, cy - 1, x + 1, cy - 2);  // arrowhead up
        canvas_draw_line(canvas, x, cy - 1, x + 1, cy);      // arrowhead down
        canvas_draw_line(canvas, x + 5, cy - 1, x + 5, cy + 2); // vertical drop
        canvas_draw_line(canvas, x + 2, cy + 2, x + 5, cy + 2); // bottom bar
        break;
    }
}

static void info_draw(Canvas* canvas, void* model) {
    if(!canvas || !model) return;
    InfoModel* m = model;
    canvas_clear(canvas);

    if(m->page == InfoPageMenu) {
        draw_header(canvas, "MENU", false);
        /* Up to 5 items in 39px of list space (y=14..53, footer at 53).
         * Hidden items collapse — we render with a visible-slot counter
         * for y-position so there's no blank row in the middle. */
        if(!info_menu_item_visible(m->index)) m->index = info_menu_step(m->index, 1);
        const int item_h = 8;
        const int list_y = 14;
        int vpos = 0;
        for(int i = 0; i < INFO_MENU_COUNT; i++) {
            if(!info_menu_item_visible(i)) continue;
            int by = list_y + vpos * item_h;
            vpos++;
            const char* label = info_menu_items[i];
            if(i == INFO_IDX_BLE) {
                label = (app_settings_get_ble_mode() == BleModeDesktop)
                            ? "Claude Desktop (BLE)"
                            : "Claude Code (USB/BLE)";
            }
            canvas_set_font(canvas, FontSecondary);
            if(i == m->index) {
                canvas_draw_rbox(canvas, 1, by - 2, 121, item_h + 1, 1);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, 5, by + 6, label);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, 5, by + 6, label);
            }
        }
        draw_footer_sep(canvas);
        hint_ok(canvas, "Open");
        hint_back(canvas, "Back");
    } else if(m->page == InfoPageHelp) {
        draw_header(canvas, "HELP", false);
        bool desktop = app_settings_get_ble_mode() == BleModeDesktop;
        int total = desktop ? HELP_DESKTOP_LINES : HELP_BRIDGE_LINES;
        int max_scroll = (total > HELP_VISIBLE) ? (total - HELP_VISIBLE) : 0;
        if(m->scroll > max_scroll) m->scroll = max_scroll;
        int y = 19;
        canvas_set_font(canvas, FontSecondary);
        for(int i = 0; i < HELP_VISIBLE && (m->scroll + i) < total; i++) {
            int idx = m->scroll + i;
            if(desktop) {
                canvas_draw_str(canvas, 2, y, help_text_desktop[idx]);
            } else {
                const HelpEntry* e = &help_entries_bridge[idx];
                if(e->button == HelpBtnText) {
                    canvas_draw_str(canvas, 2, y, e->desc);
                } else {
                    draw_help_icon(canvas, 2, y, e->button);
                    if(e->hold) canvas_draw_str(canvas, 9, y, "Hold");
                    canvas_draw_str(canvas, HELP_COLON_X, y, ":");
                    canvas_draw_str(canvas, HELP_DESC_X, y, e->desc);
                }
            }
            y += HELP_LINE_H;
        }
        if(total > HELP_VISIBLE) {
            draw_scrollbar(canvas, m->scroll, max_scroll + 1,
                            HDR_H + 1, FTR_Y - 1);
        }
        draw_footer_sep(canvas);
        hint_back(canvas, "Back");
    } else if(m->page == InfoPageAbout) {
        draw_header(canvas, "ABOUT", false);
        // Character scrolls with content — only visible at scroll 0
        int text_start, text_count, y;
        if(m->scroll == 0) {
            draw_claude(canvas, 55, 12, PoseIdle, m->anim_frame);
            text_start = 0;
            text_count = ABOUT_VISIBLE;
            y = 30;
        } else {
            text_start = m->scroll - 1;
            text_count = ABOUT_VISIBLE + 1;
            y = 19;
        }
        for(int i = 0; i < text_count && (text_start + i) < ABOUT_LINE_COUNT; i++) {
            int li = text_start + i;
            canvas_set_font(canvas, (li == 0) ? FontPrimary : FontSecondary);
            canvas_draw_str_aligned(
                canvas, 64, y, AlignCenter, AlignCenter, about_lines[li]);
            y += 9;
        }
        int about_max = 1 + ABOUT_LINE_COUNT - (ABOUT_VISIBLE + 1);
        if(about_max > 0) {
            draw_scrollbar(canvas, m->scroll, about_max + 1, HDR_H + 1, FTR_Y - 1);
        }
        draw_footer_sep(canvas);
        hint_back(canvas, "Back");
    } else if(m->page == InfoPageTranscript) {
        draw_header(canvas, "TRANSCRIPT", false);
        canvas_set_font(canvas, FontSecondary);
        if(app_settings_get_ble_mode() == BleModeDesktop) {
            /* Desktop mode: show the ring buffer (newest first).  Up/Down
             * scroll; each line is right-clipped so it doesn't collide
             * with the scrollbar track at x=125..127. */
            int total = nus_transcript_count();
            if(total == 0) {
                canvas_draw_str_aligned(canvas, 63, 31, AlignCenter, AlignCenter, "(empty)");
            } else {
                const int visible = 4;
                const int item_h = 10;
                const int text_area_w = 121; /* visible text window width */
                int max_scroll = (total > visible) ? (total - visible) : 0;
                int start = m->scroll;
                if(start < 0) start = 0;
                if(start > max_scroll) start = max_scroll;

                /* Pass 1: read the visible lines into a local buffer and
                 * find the widest one — determines the horizontal-scroll
                 * ceiling. */
                char lines[4][NUS_TRANSCRIPT_LINE_MAX];
                int widths[4] = {0};
                int shown = 0;
                int max_w = 0;
                for(int i = 0; i < visible && (start + i) < total; i++) {
                    if(nus_transcript_get(start + i, lines[i], sizeof(lines[i]))) {
                        widths[i] = (int)canvas_string_width(canvas, lines[i]);
                        if(widths[i] > max_w) max_w = widths[i];
                        shown++;
                    } else {
                        lines[i][0] = '\0';
                    }
                }
                int max_h = (max_w > text_area_w) ? (max_w - text_area_w) : 0;
                if(m->h_scroll < 0) m->h_scroll = 0;
                if(m->h_scroll > max_h) m->h_scroll = max_h;

                /* Pass 2: draw each line shifted by h_scroll. The vertical
                 * scrollbar at x=125..127 is drawn after the text and
                 * clips the right edge visually. */
                int y = 19;
                for(int i = 0; i < shown; i++) {
                    canvas_draw_str(canvas, 2 - m->h_scroll, y, lines[i]);
                    y += item_h;
                }

                if(max_scroll > 0) {
                    draw_scrollbar(canvas, start, max_scroll + 1,
                                   HDR_H + 1, FTR_Y - 1);
                }
                /* Horizontal scroll thumb: 3 px tall, straddling the
                 * footer separator (1 px above + the separator itself +
                 * 1 px below → y = FTR_Y-1 .. FTR_Y+1). Track spans
                 * x=0..93, ending right before the Back hint icon at
                 * x≈97 so the thumb can visibly reach the right edge of
                 * the available gutter. Thumb *width* mirrors the
                 * vertical thumb's *height* (same draw_scrollbar rule)
                 * so the two indicators look like a matched pair. */
                if(max_h > 0) {
                    /* Track spans the full screen width (x=0..127). The
                     * thumb lives at y=FTR_Y-1..FTR_Y+1 (52..54), which
                     * doesn't collide with the Back hint icon/label
                     * below (y=55..63), so there's no need to dodge the
                     * right side of the footer. Thumb width mirrors the
                     * vertical thumb's height for a matched pair. */
                    const int x0 = 0;
                    const int x1 = 127;
                    int v_track = (FTR_Y - 1) - (HDR_H + 1);
                    int v_count = (max_scroll > 0) ? (max_scroll + 1) : 1;
                    int tw = v_track / v_count;
                    if(tw < 4) tw = 4;
                    if(tw > x1 - x0 + 1) tw = x1 - x0 + 1;
                    /* canvas_draw_box renders [tx, tx+tw-1]; at
                     * h_scroll==max_h we want tx+tw-1 == x1. */
                    int tx = x0 + (m->h_scroll * (x1 - x0 - tw + 1)) / max_h;
                    canvas_draw_box(canvas, tx, FTR_Y - 1, tw, 3);
                }
            }
        } else {
            /* Bridge mode: this page is a remote for the host CLI — the
             * buttons here forward page-up / -down / Ctrl+O / Ctrl+E as
             * keypresses via the Python bridge. */
            int y = 19;
            draw_help_icon(canvas, 2, y, HelpBtnUp);
            canvas_draw_str(canvas, 10, y, "Page Up");
            y += HELP_LINE_H;
            draw_help_icon(canvas, 2, y, HelpBtnDown);
            canvas_draw_str(canvas, 10, y, "Page Down");
            y += HELP_LINE_H;
            draw_help_icon(canvas, 2, y, HelpBtnLeft);
            canvas_draw_str(canvas, 10, y, "Ctrl+O");
            y += HELP_LINE_H;
            draw_help_icon(canvas, 2, y, HelpBtnRight);
            canvas_draw_str(canvas, 10, y, "Ctrl+E");
        }
        draw_footer_sep(canvas);
        hint_back(canvas, "Back");
    }
}

static bool info_input(InputEvent* event, void* context) {
    if(!event || !context) return false;
    UiState* ui = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    InfoModel* m = view_get_model(ui->info_view);

    if(m->page == InfoPageMenu) {
        if(event->key == InputKeyUp) {
            m->index = info_menu_step(m->index, -1);
            view_commit_model(ui->info_view, true);
            return true;
        }
        if(event->key == InputKeyDown) {
            m->index = info_menu_step(m->index, +1);
            view_commit_model(ui->info_view, true);
            return true;
        }
        if(event->key == InputKeyOk) {
            if(m->index == INFO_IDX_BLE) {
                BleMode cur = app_settings_get_ble_mode();
                BleMode next = (cur == BleModeDesktop) ? BleModeBridge : BleModeDesktop;
                app_settings_set_ble_mode(next);
                view_commit_model(ui->info_view, true);
                if(ui->event_callback)
                    ui->event_callback(UiEventToggleBleMode, NULL, ui->event_context);
                return true;
            }
            if(m->index == INFO_IDX_SHIFTTAB) {
                view_commit_model(ui->info_view, false);
                if(ui->event_callback)
                    ui->event_callback(UiEventShiftTab, NULL, ui->event_context);
                return true;
            }
            /* Map remaining entries to their sub-pages.  BLE (0) and
             * Shift+Tab (2) are handled above; their entries here are
             * placeholders. */
            const InfoPage pages[INFO_MENU_COUNT] = {
                0, InfoPageTranscript, 0, InfoPageHelp, InfoPageAbout};
            m->page = pages[m->index];
            m->scroll = 0;
            m->h_scroll = 0;
            view_commit_model(ui->info_view, true);
            return true;
        }
        if(event->key == InputKeyBack) {
            ui->current_view = ViewIdStatus;
            view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);
            return true;
        }
    } else if(m->page == InfoPageHelp) {
        int total = (app_settings_get_ble_mode() == BleModeDesktop)
                        ? HELP_DESKTOP_LINES
                        : HELP_BRIDGE_LINES;
        int max_scroll = (total > HELP_VISIBLE) ? (total - HELP_VISIBLE) : 0;
        if(event->key == InputKeyUp && m->scroll > 0) {
            m->scroll--;
            view_commit_model(ui->info_view, true);
            return true;
        }
        if(event->key == InputKeyDown && m->scroll < max_scroll) {
            m->scroll++;
            view_commit_model(ui->info_view, true);
            return true;
        }
        if(event->key == InputKeyBack) {
            m->page = InfoPageMenu;
            view_commit_model(ui->info_view, true);
            return true;
        }
    } else if(m->page == InfoPageTranscript) {
        bool desktop = app_settings_get_ble_mode() == BleModeDesktop;
        if(desktop) {
            /* Local scroll of the ring buffer. Up/Down move through
             * the lines; Left/Right pan horizontally for long lines
             * (bounds re-clamped in the draw pass against current line
             * widths). */
            int total = nus_transcript_count();
            const int visible = 4;
            const int h_step = 30; /* ~5-6 chars per press */
            int max_scroll = (total > visible) ? (total - visible) : 0;
            if(event->key == InputKeyUp && m->scroll > 0) {
                m->scroll--;
                view_commit_model(ui->info_view, true);
                return true;
            }
            if(event->key == InputKeyDown && m->scroll < max_scroll) {
                m->scroll++;
                view_commit_model(ui->info_view, true);
                return true;
            }
            if(event->key == InputKeyLeft) {
                m->h_scroll -= h_step;
                if(m->h_scroll < 0) m->h_scroll = 0;
                view_commit_model(ui->info_view, true);
                return true;
            }
            if(event->key == InputKeyRight) {
                m->h_scroll += h_step;
                /* Upper bound enforced in draw pass (depends on visible
                 * lines' widths, which we don't know here). */
                view_commit_model(ui->info_view, true);
                return true;
            }
            if(event->key == InputKeyBack) {
                m->page = InfoPageMenu;
                m->scroll = 0;
                m->h_scroll = 0;
                view_commit_model(ui->info_view, true);
                return true;
            }
            return false;
        }
        /* Bridge mode: forward Page Up / Down / Ctrl+O / Ctrl+E to host. */
        view_commit_model(ui->info_view, false);
        if(event->key == InputKeyUp) {
            if(ui->event_callback) ui->event_callback(UiEventPageUp, NULL, ui->event_context);
            return true;
        }
        if(event->key == InputKeyDown) {
            if(ui->event_callback) ui->event_callback(UiEventPageDown, NULL, ui->event_context);
            return true;
        }
        if(event->key == InputKeyLeft) {
            if(ui->event_callback) ui->event_callback(UiEventCtrlO, NULL, ui->event_context);
            return true;
        }
        if(event->key == InputKeyRight) {
            if(ui->event_callback) ui->event_callback(UiEventCtrlE, NULL, ui->event_context);
            return true;
        }
        if(event->key == InputKeyBack) {
            InfoModel* m2 = view_get_model(ui->info_view);
            m2->page = InfoPageMenu;
            view_commit_model(ui->info_view, true);
            return true;
        }
        return true;
    } else if(m->page == InfoPageAbout) {
        if(event->key == InputKeyUp && m->scroll > 0) {
            m->scroll--;
            view_commit_model(ui->info_view, true);
            return true;
        }
        if(event->key == InputKeyDown && m->scroll < 1 + ABOUT_LINE_COUNT - (ABOUT_VISIBLE + 1)) {
            m->scroll++;
            view_commit_model(ui->info_view, true);
            return true;
        }
        if(event->key == InputKeyBack) {
            m->page = InfoPageMenu;
            view_commit_model(ui->info_view, true);
            return true;
        }
    }
    return false;
}

// ── Permission View ──────────────────────────────────────────────

static void perm_draw(Canvas* canvas, void* model) {
    if(!canvas || !model) return;
    PermModel* m = model;
    canvas_clear(canvas);

    // Header
    draw_header(canvas, "PERMISSION", true);

    // Content area: HDR_H(9) to FTR_Y(53), midpoint = 31
    int cy = (HDR_H + FTR_Y) / 2; // 31
    // Claude character (PoseWorried) on the left
    draw_claude(canvas, 4, cy - 9, PoseWorried, m->anim_frame);

    // Right column — tool on top, hint wrapped below. Character occupies
    // roughly x=4..22, so the text column starts at x≈24 with ~104 px wide.
    const int text_x = 24;
    const int text_w = 128 - text_x;

    // Tool (one line, truncate if needed — tool names are short).
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, text_x + text_w / 2, HDR_H + 7,
                            AlignCenter, AlignCenter, m->tool);

    // Hint — word-wrap under the tool.
    if(m->detail[0] != '\0') {
        canvas_set_font(canvas, FontSecondary);
        // Bridge mode reserves a row at the bottom for the Once/Always
        // toggle; Desktop uses that space for another wrapped line.
        int max_lines = m->allow_always ? 2 : 3;
        char lines[3][32];
        int nlines = wrap_text(canvas, m->detail, text_w, lines, max_lines);
        const int line_h = 8;
        int y = HDR_H + 17;  // first hint line baseline
        for(int i = 0; i < nlines; i++) {
            canvas_draw_str_aligned(
                canvas, text_x + text_w / 2, y + i * line_h,
                AlignCenter, AlignCenter, lines[i]);
        }
    }

    // Mode toggle indicator — Bridge only.
    if(m->allow_always) {
        canvas_set_font(canvas, FontSecondary);
        const char* mode_label = m->mode ? "Always" : "Once";
        int mw = (int)canvas_string_width(canvas, mode_label);
        int mx = (text_x + text_w / 2) - (mw + 9) / 2;
        int my = cy + 14;
        // Small ▼ triangle
        canvas_draw_line(canvas, mx, my, mx + 4, my);
        canvas_draw_line(canvas, mx + 1, my + 1, mx + 3, my + 1);
        canvas_draw_dot(canvas, mx + 2, my + 2);
        canvas_draw_str(canvas, mx + 7, my + 4, mode_label);
    }

    // Dark footer (hints drawn 1px higher to avoid bottom clipping)
    canvas_draw_box(canvas, 0, FTR_Y, 128, 64 - FTR_Y);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    {
        // ◄ Esc (left)
        const int y = 58;
        canvas_draw_line(canvas, 0, y, 6, y);
        canvas_draw_line(canvas, 2, y - 1, 6, y - 1);
        canvas_draw_line(canvas, 2, y + 1, 6, y + 1);
        canvas_draw_line(canvas, 4, y - 2, 6, y - 2);
        canvas_draw_line(canvas, 4, y + 2, 6, y + 2);
        canvas_draw_dot(canvas, 6, y - 3);
        canvas_draw_dot(canvas, 6, y + 3);
        canvas_draw_str_aligned(canvas, 9, y + 1, AlignLeft, AlignCenter, "Cancel");
    }
    {
        // ● Allow (center)
        int lw = (int)canvas_string_width(canvas, "Allow");
        int total = 6 + 2 + lw;
        int ix = 64 - total / 2 + 3;
        const int y = 58;
        canvas_draw_circle(canvas, ix, y, 4);
        canvas_draw_disc(canvas, ix, y, 2);
        canvas_draw_str(canvas, ix + 6, 62, "Allow");
    }
    {
        // ↩ Deny (right)
        int lw = (int)canvas_string_width(canvas, "Deny");
        int ax = 127 - lw - 11;
        int ay = 56;
        canvas_draw_line(canvas, ax + 3, ay + 4, ax + 7, ay + 4);
        canvas_draw_line(canvas, ax + 7, ay, ax + 7, ay + 4);
        canvas_draw_line(canvas, ax, ay, ax + 7, ay);
        canvas_draw_line(canvas, ax, ay, ax + 2, ay - 2);
        canvas_draw_line(canvas, ax, ay, ax + 2, ay + 2);
        canvas_draw_str(canvas, 127 - lw, 62, "Deny");
    }
    canvas_set_color(canvas, ColorBlack);
}

static bool perm_input(InputEvent* event, void* context) {
    if(!event || !context) return false;
    UiState* ui = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        PermModel* m = view_get_model(ui->perm_view);
        uint8_t mode = m->mode;
        view_commit_model(ui->perm_view, false);
        if(ui->event_callback) {
            ui->event_callback(
                mode ? UiEventPermAlways : UiEventPermAllow, NULL, ui->event_context);
        }
        return true;
    }
    if(event->key == InputKeyDown) {
        PermModel* m = view_get_model(ui->perm_view);
        if(!m->allow_always) {
            /* Desktop NUS has no "always" decision — swallow the press. */
            view_commit_model(ui->perm_view, false);
            return true;
        }
        m->mode = m->mode ? 0 : 1;
        view_commit_model(ui->perm_view, true);
        return true;
    }
    if(event->key == InputKeyBack) {
        if(ui->event_callback) ui->event_callback(UiEventPermDeny, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyLeft) {
        if(ui->event_callback) ui->event_callback(UiEventPermEsc, NULL, ui->event_context);
        return true;
    }
    return false;
}

// ── Public API ───────────────────────────────────────────────────

UiState* ui_alloc(Gui* gui) {
    UiState* ui = malloc(sizeof(UiState));
    furi_check(ui != NULL);
    memset(ui, 0, sizeof(UiState));

    ui->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(ui->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // Status view
    ui->status_view = view_alloc();
    view_allocate_model(ui->status_view, ViewModelTypeLockFree, sizeof(StatusModel));
    view_set_draw_callback(ui->status_view, status_draw);
    view_set_input_callback(ui->status_view, status_input);
    view_set_context(ui->status_view, ui);
    view_dispatcher_add_view(ui->view_dispatcher, ViewIdStatus, ui->status_view);

    // Menu view
    ui->menu_view = view_alloc();
    view_allocate_model(ui->menu_view, ViewModelTypeLockFree, sizeof(MenuModel));
    view_set_draw_callback(ui->menu_view, menu_draw);
    view_set_input_callback(ui->menu_view, menu_input);
    view_set_context(ui->menu_view, ui);
    {
        MenuModel* m = view_get_model(ui->menu_view);
        for(int i = 0; i < DEFAULT_MENU_COUNT && i < MAX_MENU_ITEMS; i++) {
            strncpy(m->items[i], default_menu_items[i], MAX_MENU_ITEM_LEN - 1);
            m->items[i][MAX_MENU_ITEM_LEN - 1] = '\0';
        }
        m->count = DEFAULT_MENU_COUNT;
        m->index = 0;
    }
    view_dispatcher_add_view(ui->view_dispatcher, ViewIdMenu, ui->menu_view);

    // Permission view
    ui->perm_view = view_alloc();
    view_allocate_model(ui->perm_view, ViewModelTypeLockFree, sizeof(PermModel));
    view_set_draw_callback(ui->perm_view, perm_draw);
    view_set_input_callback(ui->perm_view, perm_input);
    view_set_context(ui->perm_view, ui);
    view_dispatcher_add_view(ui->view_dispatcher, ViewIdPerm, ui->perm_view);

    // Info view
    ui->info_view = view_alloc();
    view_allocate_model(ui->info_view, ViewModelTypeLockFree, sizeof(InfoModel));
    view_set_draw_callback(ui->info_view, info_draw);
    view_set_input_callback(ui->info_view, info_input);
    view_set_context(ui->info_view, ui);
    view_dispatcher_add_view(ui->view_dispatcher, ViewIdInfo, ui->info_view);

    // Animation timer (150ms tick for character animations)
    ui->anim_timer = furi_timer_alloc(anim_tick, FuriTimerTypePeriodic, ui);
    furi_timer_start(ui->anim_timer, 150);

    // Start on status
    ui->current_view = ViewIdStatus;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);

    return ui;
}

void ui_free(UiState* ui) {
    if(!ui) return;
    furi_timer_stop(ui->anim_timer);
    furi_timer_free(ui->anim_timer);
    view_dispatcher_remove_view(ui->view_dispatcher, ViewIdStatus);
    view_dispatcher_remove_view(ui->view_dispatcher, ViewIdMenu);
    view_dispatcher_remove_view(ui->view_dispatcher, ViewIdPerm);
    view_dispatcher_remove_view(ui->view_dispatcher, ViewIdInfo);
    view_free(ui->status_view);
    view_free(ui->menu_view);
    view_free(ui->perm_view);
    view_free(ui->info_view);
    view_dispatcher_free(ui->view_dispatcher);
    free(ui);
}

void ui_set_event_callback(UiState* ui, UiEventCallback callback, void* context) {
    if(!ui) return;
    ui->event_callback = callback;
    ui->event_context = context;
}

void ui_show_status(UiState* ui, const char* text, bool connected) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->connected = connected;
    if(text) {
        strncpy(m->text, text, sizeof(m->text) - 1);
        m->text[sizeof(m->text) - 1] = '\0';
    } else {
        m->text[0] = '\0';
    }
    m->subtext[0] = '\0';
    if(!connected) {
        m->pose = PoseSleeping;
        m->anim_frame = 0;
    }
    view_commit_model(ui->status_view, true);
    if(ui->current_view != ViewIdMenu && ui->current_view != ViewIdInfo) {
        ui->current_view = ViewIdStatus;
        view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);
    }
}

void ui_show_status2(UiState* ui, const char* text, const char* subtext, bool connected) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->connected = connected;
    if(text) {
        strncpy(m->text, text, sizeof(m->text) - 1);
        m->text[sizeof(m->text) - 1] = '\0';
    } else {
        m->text[0] = '\0';
    }
    if(subtext) {
        strncpy(m->subtext, subtext, sizeof(m->subtext) - 1);
        m->subtext[sizeof(m->subtext) - 1] = '\0';
    } else {
        m->subtext[0] = '\0';
    }
    if(!connected) {
        m->pose = PoseSleeping;
        m->anim_frame = 0;
    }
    view_commit_model(ui->status_view, true);
    if(ui->current_view != ViewIdMenu && ui->current_view != ViewIdInfo) {
        ui->current_view = ViewIdStatus;
        view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);
    }
}

void ui_show_menu(UiState* ui) {
    if(!ui) return;
    MenuModel* m = view_get_model(ui->menu_view);
    m->index = 0;
    view_commit_model(ui->menu_view, true);
    ui->current_view = ViewIdMenu;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdMenu);
}

void ui_show_info(UiState* ui) {
    if(!ui) return;
    InfoModel* m = view_get_model(ui->info_view);
    m->page = InfoPageMenu;
    m->index = 0;
    view_commit_model(ui->info_view, true);
    ui->current_view = ViewIdInfo;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdInfo);
}

void ui_show_listening(UiState* ui) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->connected = true;
    strncpy(m->text, "Listening...", sizeof(m->text) - 1);
    m->text[sizeof(m->text) - 1] = '\0';
    m->subtext[0] = '\0';
    m->pose = PoseListening;
    m->anim_frame = 0;
    view_commit_model(ui->status_view, true);
    ui->current_view = ViewIdStatus;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);
}

void ui_set_claude_connected(UiState* ui, bool connected) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->claude_connected = connected;
    if(!connected) {
        m->rssi_bars = 0; // 清零信号格
    }
    view_commit_model(ui->status_view, true);
}

void ui_set_transport_mode(UiState* ui, bool is_bt) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->transport_mode = is_bt ? 1 : 0;
    if(!is_bt) {
        m->rssi_bars = 0;
    }
    view_commit_model(ui->status_view, true);
}

void ui_set_rssi(UiState* ui, int rssi) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    if(m->transport_mode != 1) {
        m->rssi_bars = 0;
    } else {
        m->rssi_bars = rssi_to_bars(rssi);
    }
    view_commit_model(ui->status_view, true);
}

void ui_set_pose(UiState* ui, uint8_t pose) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    // PoseListening is sticky: only voice_stop (PoseIdle) can clear it
    if(m->pose == PoseListening && pose != PoseIdle && pose != PoseListening) {
        view_commit_model(ui->status_view, false);
        return;
    }
    m->pose = pose;
    m->anim_frame = 0;
    view_commit_model(ui->status_view, true);
}

void ui_update_menu(UiState* ui, const char* pipe_delimited) {
    if(!ui || !pipe_delimited) return;
    MenuModel* m = view_get_model(ui->menu_view);
    m->count = 0;
    m->index = 0;

    const char* p = pipe_delimited;
    while(*p && m->count < MAX_MENU_ITEMS) {
        const char* sep = strchr(p, '|');
        int len = sep ? (int)(sep - p) : (int)strlen(p);
        if(len > MAX_MENU_ITEM_LEN - 1) len = MAX_MENU_ITEM_LEN - 1;
        if(len > 0) {
            memcpy(m->items[m->count], p, len);
            m->items[m->count][len] = '\0';
            m->count++;
        }
        if(!sep) break;
        p = sep + 1;
    }

    view_commit_model(ui->menu_view, true);
}

void ui_show_permission(UiState* ui, const char* tool, const char* detail, bool allow_always) {
    if(!ui) return;
    PermModel* m = view_get_model(ui->perm_view);
    if(tool) {
        strncpy(m->tool, tool, sizeof(m->tool) - 1);
        m->tool[sizeof(m->tool) - 1] = '\0';
    } else {
        m->tool[0] = '\0';
    }
    if(detail) {
        strncpy(m->detail, detail, sizeof(m->detail) - 1);
        m->detail[sizeof(m->detail) - 1] = '\0';
    } else {
        m->detail[0] = '\0';
    }
    m->anim_frame = 0;
    m->allow_always = allow_always;
    /* Without a toggle there's no "Always" decision to preserve — reset to
     * Once so a stale toggle state from a previous Bridge-mode prompt
     * doesn't bleed through. */
    if(!allow_always) m->mode = 0;
    view_commit_model(ui->perm_view, true);
    ui->current_view = ViewIdPerm;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdPerm);
}

void ui_back_to_status(UiState* ui) {
    if(!ui) return;
    ui->current_view = ViewIdStatus;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);
}

void ui_set_muted(UiState* ui, bool muted) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->muted = muted;
    view_commit_model(ui->status_view, true);
}

void ui_stop(UiState* ui) {
    if(!ui) return;
    view_dispatcher_stop(ui->view_dispatcher);
}

void ui_run(UiState* ui) {
    if(!ui) return;
    view_dispatcher_run(ui->view_dispatcher);
}
