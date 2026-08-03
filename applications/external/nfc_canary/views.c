#include "views.h"
#include "alert.h"
#include "eventlog.h"
#include "settings_menu.h"

#include <furi_hal_rtc.h>

#define SCREEN_W 128
#define SCREEN_H 64

static void fmt_clock(uint32_t ts, char* out, size_t len) {
    /* Timestamps are seconds-of-day from the RTC; wall clock is what matters
     * for "when did this happen", not an absolute epoch. */
    uint32_t h = (ts / 3600) % 24;
    uint32_t m = (ts / 60) % 60;
    uint32_t s = ts % 60;
    snprintf(out, len, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

/* ---------- history strip ----------
 * One column per time slot, height by highest tier seen in that slot. Gives an
 * at-a-glance answer to "was that one reader or five?" without opening the log.
 */
static void draw_history(Canvas* canvas, AlerterState* state, int y) {
    canvas_draw_line(canvas, 0, y + 9, SCREEN_W - 1, y + 9);

    for(int i = 0; i < HISTORY_SLOTS; i++) {
        uint8_t tier = state->history[i];
        if(!tier) continue;

        int h = (tier == TierInfo) ? 2 : (tier == TierWarn) ? 5 : 9;
        int x = i * 2;
        canvas_draw_box(canvas, x, y + 9 - h, 2, h);
    }
}

/* ---------- screens ---------- */

static void draw_intro(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "Detects READERS");
    canvas_draw_line(canvas, 0, 15, 127, 15);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 26, "A card or tag will NOT");
    canvas_draw_str(canvas, 2, 35, "trigger this - tags have");
    canvas_draw_str(canvas, 2, 44, "no transmitter.");
    canvas_draw_str(canvas, 2, 55, "Use a phone (NFC on) or");
    canvas_draw_str(canvas, 2, 63, "terminal, on the BACK.");
}

static void draw_status(Canvas* canvas, AlerterState* state, const AlerterSettings* settings) {
    bool field = state->field_present;
    ThreatTier tier = state->current_tier;

    if(field) {
        canvas_draw_box(canvas, 0, 0, SCREEN_W, 20);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        const char* label = (tier == TierAlarm) ? "! SCANNING YOU !" : "READER NEARBY";
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, label);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignCenter, "NFC Canary");
        canvas_draw_line(canvas, 0, 18, 127, 18);
    }

    canvas_set_font(canvas, FontSecondary);

    char line[40];
    if(field) {
        uint32_t ms = (furi_get_tick() - state->field_start_tick);
        snprintf(
            line,
            sizeof(line),
            "%s  %lu.%lus",
            alert_tier_name(tier),
            (unsigned long)(ms / 1000),
            (unsigned long)((ms % 1000) / 100));
    } else {
        /* Mode is not user-selectable yet, so naming it here would be noise. */
        snprintf(line, sizeof(line), "Armed - listening");
    }
    canvas_draw_str(canvas, 2, 29, line);

    /* Protocol line only carries information in Decoy mode. */
    if(settings->mode == ModeDecoy && state->last_protocol != 0xFF) {
        snprintf(line, sizeof(line), "%s", eventlog_protocol_name(state->last_protocol));
        canvas_draw_str(canvas, 2, 38, line);
        if(state->last_cmd) {
            snprintf(line, sizeof(line), "%s", eventlog_cmd_name(state->last_cmd));
            canvas_draw_str(canvas, 2, 46, line);
        }
    } else {
        snprintf(
            line,
            sizeof(line),
            "Events %lu  A:%lu W:%lu",
            (unsigned long)state->event_count,
            (unsigned long)state->count_alarm,
            (unsigned long)state->count_warn);
        canvas_draw_str(canvas, 2, 38, line);
    }

    draw_history(canvas, state, 44);

    canvas_set_font(canvas, FontSecondary);
    /* Spell the destinations out. Abbreviations save pixels but cost the user
     * a guess every time they look at it. */
    canvas_draw_str(canvas, 2, 62, "<Session");
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "OK:Setup");
    canvas_draw_str_aligned(canvas, 127, 62, AlignRight, AlignBottom, "Events>");
}

static void draw_events(Canvas* canvas, const ViewState* view, AlerterState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, "Event Log");
    canvas_draw_line(canvas, 0, 13, 127, 13);

    uint16_t count = eventlog_count_locked(state);
    canvas_set_font(canvas, FontSecondary);

    if(count == 0) {
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "No events yet");
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Back");
        return;
    }

    /* Four rows, scrolling window centred on the cursor. */
    const int rows = 4;
    int top = view->event_cursor - rows / 2;
    if(top < 0) top = 0;
    if(top > count - rows) top = count - rows;
    if(top < 0) top = 0;

    for(int i = 0; i < rows && (top + i) < count; i++) {
        AlertEvent ev;
        if(!eventlog_get_locked(state, top + i, &ev)) continue;

        int y = 22 + i * 10;
        bool sel = (top + i) == view->event_cursor;

        if(sel) {
            canvas_draw_box(canvas, 0, y - 8, SCREEN_W, 10);
            canvas_set_color(canvas, ColorWhite);
        }

        char clock[12];
        fmt_clock(ev.timestamp, clock, sizeof(clock));

        char line[40];
        snprintf(line, sizeof(line), "%s %s", clock, alert_tier_name((ThreatTier)ev.tier));
        canvas_draw_str(canvas, 2, y, line);

        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    char foot[32];
    snprintf(foot, sizeof(foot), "%u/%u  OK:Detail", view->event_cursor + 1, count);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, foot);
}

static void draw_event_detail(Canvas* canvas, const ViewState* view, AlerterState* state) {
    AlertEvent ev;
    if(!eventlog_get_locked(state, view->event_cursor, &ev)) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Event gone");
        return;
    }

    char clock[12];
    fmt_clock(ev.timestamp, clock, sizeof(clock));

    canvas_set_font(canvas, FontPrimary);
    char head[32];
    snprintf(head, sizeof(head), "%s  %s", clock, alert_tier_name((ThreatTier)ev.tier));
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, head);
    canvas_draw_line(canvas, 0, 13, 127, 13);

    canvas_set_font(canvas, FontSecondary);
    char line[40];

    snprintf(line, sizeof(line), "Exposure: %u ms", (unsigned)ev.duration_ms);
    canvas_draw_str(canvas, 2, 24, line);

    /* Protocol and command are only populated in Decoy mode. Until that lands,
     * printing "Unknown / none" on every event is noise, so show the detail
     * only when there is detail to show. */
    if(ev.protocol != 0xFF) {
        snprintf(line, sizeof(line), "Proto: %s", eventlog_protocol_name(ev.protocol));
        canvas_draw_str(canvas, 2, 33, line);
    }
    if(ev.cmd) {
        snprintf(line, sizeof(line), "Cmd %02X: %s", ev.cmd, eventlog_cmd_name(ev.cmd));
        canvas_draw_str(canvas, 2, 42, line);
    }
    if(ev.protocol == 0xFF && !ev.cmd) {
        canvas_draw_str(canvas, 2, 33, "Carrier detected");
        canvas_draw_str(canvas, 2, 42, "(no protocol detail)");
    }

    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Back");
}

static void draw_session(Canvas* canvas, AlerterState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, "Session");
    canvas_draw_line(canvas, 0, 13, 127, 13);

    canvas_set_font(canvas, FontSecondary);
    char line[40];

    snprintf(
        line,
        sizeof(line),
        "Alarm %lu  Warn %lu  Info %lu",
        (unsigned long)state->count_alarm,
        (unsigned long)state->count_warn,
        (unsigned long)state->count_info);
    canvas_draw_str(canvas, 2, 24, line);

    if(state->first_event) {
        char a[12], b[12];
        fmt_clock(state->first_event, a, sizeof(a));
        fmt_clock(state->last_event, b, sizeof(b));
        snprintf(line, sizeof(line), "First %s", a);
        canvas_draw_str(canvas, 2, 34, line);
        snprintf(line, sizeof(line), "Last  %s", b);
        canvas_draw_str(canvas, 2, 43, line);
        snprintf(line, sizeof(line), "Longest %lu ms", (unsigned long)state->longest_ms);
        canvas_draw_str(canvas, 2, 52, line);
    } else {
        canvas_draw_str(canvas, 2, 36, "No events this session");
    }

    canvas_draw_str(canvas, 2, 62, "OK:Clear");
    canvas_draw_str_aligned(canvas, 127, 62, AlignRight, AlignBottom, "Status>");
}

static void draw_settings(Canvas* canvas, const ViewState* view, const AlerterSettings* settings) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, "Settings");
    canvas_draw_line(canvas, 0, 13, 127, 13);

    SettingRow rows[16];
    uint8_t n = settings_menu_build((AlerterSettings*)settings, rows, 16);

    canvas_set_font(canvas, FontSecondary);

    const int visible = 4;
    int top = view->settings_top;
    if(view->settings_cursor < top) top = view->settings_cursor;
    if(view->settings_cursor >= top + visible) top = view->settings_cursor - visible + 1;

    for(int i = 0; i < visible && (top + i) < n; i++) {
        const SettingRow* r = &rows[top + i];
        int y = 23 + i * 10;
        bool sel = (top + i) == view->settings_cursor;

        if(sel) {
            canvas_draw_box(canvas, 0, y - 8, SCREEN_W, 10);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_str(canvas, 2, y, r->label);

        char val[16];
        if(r->fmt) {
            snprintf(val, sizeof(val), "%s", r->fmt(*r->field));
        } else {
            snprintf(val, sizeof(val), "%u", (unsigned)*r->field);
        }
        canvas_draw_str_aligned(canvas, SCREEN_W - 2, y, AlignRight, AlignBottom, val);

        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    /* Left/Right editing is not discoverable without saying so. */
    canvas_draw_str(canvas, 2, 62, "<>Change");
    canvas_draw_str_aligned(canvas, 127, 62, AlignRight, AlignBottom, "OK:Test");
}

static void draw_diag(Canvas* canvas, AlerterState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "Diagnostics");
    canvas_draw_line(canvas, 0, 15, 127, 15);

    canvas_set_font(canvas, FontSecondary);
    char line[40];

    snprintf(line, sizeof(line), "NFC radio:  %s", state->nfc_ok ? "OK" : "FAILED");
    canvas_draw_str(canvas, 2, 26, line);
    snprintf(
        line,
        sizeof(line),
        "EFD %s   Latch %s",
        state->raw_field ? "1" : "0",
        state->field_present ? "ON" : "off");
    canvas_draw_str(canvas, 2, 36, line);
    snprintf(line, sizeof(line), "Raw edges:  %lu", (unsigned long)state->raw_edges);
    canvas_draw_str(canvas, 2, 46, line);
    snprintf(line, sizeof(line), "Detections: %lu", (unsigned long)state->detections);
    canvas_draw_str(canvas, 2, 56, line);

    canvas_draw_str_aligned(canvas, 127, 62, AlignRight, AlignBottom, "OK:Test");
}

/* ---------- dispatch ---------- */

void views_draw(
    Canvas* canvas,
    const ViewState* view,
    AlerterState* state,
    const AlerterSettings* settings) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    furi_mutex_acquire(state->mutex, FuriWaitForever);

    switch(view->screen) {
    case ScreenIntro:
        draw_intro(canvas);
        break;
    case ScreenStatus:
        draw_status(canvas, state, settings);
        break;
    case ScreenEvents:
        draw_events(canvas, view, state);
        break;
    case ScreenEventDetail:
        draw_event_detail(canvas, view, state);
        break;
    case ScreenSession:
        draw_session(canvas, state);
        break;
    case ScreenSettings:
        draw_settings(canvas, view, settings);
        break;
    case ScreenDiag:
        draw_diag(canvas, state);
        break;
    }

    furi_mutex_release(state->mutex);
}
