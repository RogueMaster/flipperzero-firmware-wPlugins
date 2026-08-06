#include "tpms_view.h"

#include <furi.h>
#include <gui/elements.h>

/* List layout: header, 10 px rows, footer with key hints. */
#define TPMS_ROW_TOP    14
#define TPMS_ROW_HEIGHT 10
#define TPMS_LIST_LEFT  2

/* Columns of a list row. Exact dBm live on the detail screen: there is no
 * room for them in a row that would not be taken from the readings. */
#define TPMS_COL_PRESSURE 38
#define TPMS_COL_TEMP     72
#define TPMS_COL_BARS     104

/** Bars in the signal level indicator. */
#define TPMS_BAR_COUNT 4
#define TPMS_BAR_WIDTH 3
#define TPMS_BAR_STEP  4

/* Lines never exceed the screen width, but keep some headroom anyway. */
#define TPMS_TEXT_MAX 48

/** Level in bars. Thresholds are tuned for a sensor inside a wheel: right
 * against the Flipper that is about -60 dBm, across the car body about
 * -90 dBm. */
static uint8_t tpms_view_bars(int16_t rssi_x10) {
    const int32_t dbm = rssi_x10 / 10;
    if(dbm >= -65) return 4;
    if(dbm >= -75) return 3;
    if(dbm >= -85) return 2;
    if(dbm >= -95) return 1;
    return 0;
}

/** Ladder of bars. Stale readings are drawn as outlines: the value is old
 * because the sensor has been silent for a while. */
static void tpms_view_draw_signal(
    Canvas* canvas,
    int32_t x,
    int32_t bottom,
    uint8_t bars,
    bool filled) {
    for(uint8_t i = 0; i < TPMS_BAR_COUNT; i++) {
        const int32_t height = 2 + i * 2;
        const int32_t bar_x = x + i * TPMS_BAR_STEP;

        if(i < bars) {
            if(filled) {
                canvas_draw_box(canvas, bar_x, bottom - height, TPMS_BAR_WIDTH, height);
            } else {
                canvas_draw_frame(canvas, bar_x, bottom - height, TPMS_BAR_WIDTH, height);
            }
        } else {
            canvas_draw_dot(canvas, bar_x + 1, bottom - 1);
        }
    }
}

/** Pressure in bar, two decimals, without floating point:
 * raw * 0.75 -> kPa, kPa / 100 -> bar. */
static void tpms_view_format_bar(char* out, size_t size, uint16_t pressure_raw) {
    const uint32_t bar_x100 = (uint32_t)pressure_raw * 75UL / 100UL;
    snprintf(out, size, "%lu.%02lu", (unsigned long)(bar_x100 / 100), (unsigned long)(bar_x100 % 100));
}

/** A signed value in tenths -> "-86.5". */
static void tpms_view_format_x10(char* out, size_t size, int32_t value_x10) {
    const int32_t magnitude = value_x10 < 0 ? -value_x10 : value_x10;
    snprintf(
        out,
        size,
        "%s%ld.%ld",
        value_x10 < 0 ? "-" : "",
        (long)(magnitude / 10),
        (long)(magnitude % 10));
}

static void tpms_view_format_age(char* out, size_t size, uint32_t age_ticks) {
    const uint32_t seconds = age_ticks / furi_ms_to_ticks(1000);
    if(seconds < 100) {
        snprintf(out, size, "%lus", (unsigned long)seconds);
    } else {
        snprintf(out, size, "%lum", (unsigned long)(seconds / 60));
    }
}

static void tpms_view_draw_header(Canvas* canvas, TpmsBridgeApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, TPMS_LIST_LEFT, 10, "TPMS");

    const char* radio = "off";
    if(app->exit_blocked) {
        radio = "wait";
    } else if(app->usb_streaming) {
        radio = "USB";
    } else if(app->local_rx) {
        radio = "RX";
    }

    char status[TPMS_TEXT_MAX];
    snprintf(
        status,
        sizeof(status),
        "%u dev  %s%s",
        (unsigned)app->store.count,
        radio,
        app->auto_wake ? " wake" : "");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, status);
    canvas_draw_line(canvas, 0, 12, 127, 12);
}

static void tpms_view_draw_row(
    Canvas* canvas,
    const TpmsSensor* sensor,
    int32_t top,
    bool selected,
    bool fresh) {
    if(selected) {
        canvas_draw_box(canvas, 0, top, 124, TPMS_ROW_HEIGHT);
        canvas_set_color(canvas, ColorWhite);
    }

    const int32_t baseline = top + 8;
    char text[TPMS_TEXT_MAX];

    snprintf(text, sizeof(text), "%06lx", (unsigned long)sensor->id);
    canvas_draw_str(canvas, TPMS_LIST_LEFT, baseline, text);

    char bar[16];
    tpms_view_format_bar(bar, sizeof(bar), sensor->pressure_raw);
    snprintf(text, sizeof(text), "%sb", bar);
    canvas_draw_str(canvas, TPMS_COL_PRESSURE, baseline, text);

    snprintf(text, sizeof(text), "%dC", (int)sensor->temperature_c);
    canvas_draw_str(canvas, TPMS_COL_TEMP, baseline, text);

    tpms_view_draw_signal(
        canvas, TPMS_COL_BARS, top + TPMS_ROW_HEIGHT - 1, tpms_view_bars(sensor->rssi_x10), fresh);

    if(selected) canvas_set_color(canvas, ColorBlack);
}

static void tpms_view_draw_list(Canvas* canvas, TpmsBridgeApp* app) {
    tpms_view_draw_header(canvas, app);
    canvas_set_font(canvas, FontSecondary);

    if(app->store.count == 0) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, "Listening...");
        canvas_draw_str_aligned(
            canvas, 64, 42, AlignCenter, AlignBottom, "Right: wake sensor");
    } else {
        const uint32_t now = furi_get_tick();
        for(uint8_t row = 0; row < TPMS_VIEW_ROWS; row++) {
            const uint8_t index = app->scroll + row;
            if(index >= app->store.count) break;

            const TpmsSensor* sensor = &app->store.items[index];
            tpms_view_draw_row(
                canvas,
                sensor,
                TPMS_ROW_TOP + row * TPMS_ROW_HEIGHT,
                index == app->selected,
                !tpms_sensor_is_stale(sensor, now));
        }

        if(app->store.count > TPMS_VIEW_ROWS) {
            elements_scrollbar_pos(
                canvas,
                128,
                TPMS_ROW_TOP,
                TPMS_VIEW_ROWS * TPMS_ROW_HEIGHT,
                app->selected,
                app->store.count);
        }
    }

    /* Spread the hints across the width: they do not fit as one string. */
    canvas_draw_line(canvas, 0, 54, 127, 54);
    canvas_draw_str(canvas, TPMS_LIST_LEFT, 62, "OK:info");
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "R:wake");
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "L:auto");
}

static void tpms_view_draw_detail(Canvas* canvas, TpmsBridgeApp* app) {
    const TpmsSensor* sensor = &app->store.items[app->selected];
    char text[TPMS_TEXT_MAX];
    char left[16];
    char right[16];

    canvas_set_font(canvas, FontPrimary);
    snprintf(
        text,
        sizeof(text),
        "#%u  ID %06lx",
        (unsigned)(app->selected + 1),
        (unsigned long)sensor->id);
    canvas_draw_str(canvas, TPMS_LIST_LEFT, 10, text);

    tpms_view_draw_signal(
        canvas,
        TPMS_COL_BARS,
        11,
        tpms_view_bars(sensor->rssi_x10),
        !tpms_sensor_is_stale(sensor, furi_get_tick()));
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);

    /* Pressure is shown in all three units: bar is what most people use,
     * PSI is printed on the sensors themselves, kPa is what the protocol
     * carries. */
    const uint32_t kpa_x100 = (uint32_t)sensor->pressure_raw * 75UL;
    const uint32_t psi_x10 = kpa_x100 * 145UL / 10000UL;

    tpms_view_format_bar(left, sizeof(left), sensor->pressure_raw);
    snprintf(text, sizeof(text), "%s bar", left);
    canvas_draw_str(canvas, TPMS_LIST_LEFT, 22, text);
    tpms_view_format_x10(right, sizeof(right), (int32_t)psi_x10);
    snprintf(text, sizeof(text), "%s PSI", right);
    canvas_draw_str_aligned(canvas, 126, 22, AlignRight, AlignBottom, text);

    snprintf(
        text,
        sizeof(text),
        "%lu.%02lu kPa",
        (unsigned long)(kpa_x100 / 100),
        (unsigned long)(kpa_x100 % 100));
    canvas_draw_str(canvas, TPMS_LIST_LEFT, 32, text);
    snprintf(text, sizeof(text), "%d C", (int)sensor->temperature_c);
    canvas_draw_str_aligned(canvas, 126, 32, AlignRight, AlignBottom, text);

    tpms_view_format_x10(left, sizeof(left), sensor->rssi_x10);
    snprintf(text, sizeof(text), "%s dBm", left);
    canvas_draw_str(canvas, TPMS_LIST_LEFT, 42, text);
    /* Whole dBm are enough for the peak: it is there to compare wheels on
     * the move, not to measure precisely. */
    snprintf(text, sizeof(text), "peak %ld", (long)(sensor->peak_rssi_x10 / 10));
    canvas_draw_str_aligned(canvas, 126, 42, AlignRight, AlignBottom, text);

    snprintf(text, sizeof(text), "frames %lu", (unsigned long)sensor->frames);
    canvas_draw_str(canvas, TPMS_LIST_LEFT, 52, text);
    tpms_view_format_age(left, sizeof(left), furi_get_tick() - sensor->last_tick);
    snprintf(text, sizeof(text), "age %s", left);
    canvas_draw_str_aligned(canvas, 126, 52, AlignRight, AlignBottom, text);

    snprintf(
        text,
        sizeof(text),
        "flags 0x%02x  aux %04x",
        (unsigned)sensor->flags,
        (unsigned)sensor->unknown);
    canvas_draw_str(canvas, TPMS_LIST_LEFT, 62, text);
}

void tpms_view_draw(Canvas* canvas, TpmsBridgeApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(app->screen == TpmsScreenDetail && app->selected < app->store.count) {
        tpms_view_draw_detail(canvas, app);
    } else {
        tpms_view_draw_list(canvas, app);
    }
}

void tpms_view_follow_selection(TpmsBridgeApp* app) {
    if(app->store.count == 0) {
        app->selected = 0;
        app->scroll = 0;
        return;
    }

    if(app->selected >= app->store.count) app->selected = app->store.count - 1;

    if(app->selected < app->scroll) {
        app->scroll = app->selected;
    } else if(app->selected >= app->scroll + TPMS_VIEW_ROWS) {
        app->scroll = app->selected - TPMS_VIEW_ROWS + 1;
    }

    if(app->store.count <= TPMS_VIEW_ROWS) app->scroll = 0;
}
