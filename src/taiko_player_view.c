// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_player_view.h"
#include "taiko_mixer.h"

#include <furi.h>
#include <gui/elements.h>
#include <stdio.h>

#define LANE_TOP           11
#define LANE_BOTTOM        27
#define LANE_Y             19
#define DRUM_X             18
#define DRUM_Y             40
#define DRUM_INNER_RADIUS  7
#define DRUM_OUTER_RADIUS  11
#define BALLOON_MAX_RADIUS 18

static void draw_menu(Canvas* canvas, const TaikoMenu* menu) {
    static const char* main_labels[] = {
        "Return",
        "Restart",
        "Play New Song",
        "Difficulty",
        "Playback Speed",
        "Scroll Speed",
        "Frame Rate",
        "Seek Mode",
        "Backward Interval",
        "Forward Interval",
        "Dong Volume",
        "Ka Volume",
        "GPIO Output",
        "GPIO Setting",
        "Restore Defaults",
        "Exit"};
    static const char* gpio_labels[] = {
        "Return", "Left Ka", "Left Dong", "Right Dong", "Right Ka", "Pulse Width"};
    const char* const* labels = menu->gpio_settings ? gpio_labels : main_labels;
    const size_t count = menu->gpio_settings ? TaikoGpioMenuItemCount : TaikoMenuItemCount;
    const size_t selection = menu->gpio_settings ? (size_t)menu->gpio_selected :
                                                   (size_t)menu->selected;
    const size_t visible = 5;
    const size_t first = selection >= visible ? selection - visible + 1 : 0;
    for(size_t i = first; i < first + visible && i < count; i++) {
        const int y = 10 + (i - first) * 12;
        const bool selected = i == selection;
        if(selected) {
            canvas_draw_rbox(canvas, 1, y - 9, 122, 12, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        const char* label = labels[i];
        if(!menu->gpio_settings && menu->seek_mode == TaikoSeekByBars) {
            if(i == TaikoMenuBackward) label = "Backward Bars";
            if(i == TaikoMenuForward) label = "Forward Bars";
        }
        canvas_draw_str(canvas, 6, y, label);
        char value[16] = "";
        if(menu->gpio_settings) {
            if(i == TaikoGpioMenuPulseWidth)
                snprintf(value, sizeof(value), "< %lums >", menu->gpio.pulse_ms);
            else if(i >= TaikoGpioMenuLeftKa && i <= TaikoGpioMenuRightKa)
                snprintf(
                    value, sizeof(value), "< %s >", taiko_gpio_pin_name(menu->gpio.pins[i - 1]));
        } else if(i == TaikoMenuGpioOutput) {
            snprintf(value, sizeof(value), "< %s >", menu->gpio.enabled ? "On" : "Off");
        } else if(i == TaikoMenuGpioSetting) {
            snprintf(value, sizeof(value), ">");
        } else if(i >= TaikoMenuDifficulty && i <= TaikoMenuKaVolume) {
            if(i == TaikoMenuDifficulty) {
                snprintf(value, sizeof(value), "< %s >", tja_course_name(menu->course));
            } else if(i == TaikoMenuPlaybackSpeed || i == TaikoMenuScrollSpeed) {
                unsigned speed = i == TaikoMenuPlaybackSpeed ? taiko_menu_playback_tenths(menu) :
                                                               taiko_menu_scroll_tenths(menu);
                snprintf(value, sizeof(value), "< %u.%ux >", speed / 10, speed % 10);
            } else if(i == TaikoMenuFrameRate) {
                snprintf(value, sizeof(value), "< %u FPS >", (unsigned)menu->frame_rate);
            } else if(i == TaikoMenuSeekMode) {
                snprintf(
                    value,
                    sizeof(value),
                    "< %s >",
                    menu->seek_mode == TaikoSeekByBars ? "By Bars" : "By Time");
            } else if(i == TaikoMenuBackward || i == TaikoMenuForward) {
                if(menu->seek_mode == TaikoSeekByBars) {
                    uint32_t bars = taiko_menu_bar_count(menu, i == TaikoMenuForward);
                    snprintf(value, sizeof(value), "< %lu >", bars);
                } else {
                    const uint32_t interval = i == TaikoMenuBackward ?
                                                  taiko_menu_backward_ms(menu) :
                                                  taiko_menu_forward_ms(menu);
                    snprintf(value, sizeof(value), "< %lus >", interval / 1000);
                }
            } else {
                unsigned volume = i == TaikoMenuDonVolume ? menu->don_volume : menu->ka_volume;
                snprintf(value, sizeof(value), "< %u%% >", volume);
            }
        }
        if(*value) canvas_draw_str(canvas, 120 - canvas_string_width(canvas, value), y, value);
        canvas_set_color(canvas, ColorBlack);
    }
    elements_scrollbar(canvas, selection, count);
}

static void draw_control(Canvas* canvas, const char* text, Align align) {
    static const uint8_t left[] = {0x08, 0x0c, 0x0e, 0x0f, 0x0e, 0x0c, 0x08};
    static const uint8_t right[] = {0x01, 0x03, 0x07, 0x0f, 0x07, 0x03, 0x01};
    static const uint8_t center[] = {0x1c, 0x22, 0x5d, 0x5d, 0x5d, 0x22, 0x1c};
    const uint8_t* icon = align == AlignLeft ? left : align == AlignRight ? right : center;
    const int padding = align == AlignCenter ? 1 : 3;
    const int icon_width = align == AlignCenter ? 7 : 4;
    const int width = canvas_string_width(canvas, text) + padding * 2 + icon_width + 3;
    /* Native buttons are bottom-aligned; use a baseline one pixel lower. */
    const int bottom = canvas_height(canvas) + 1;
    const int top = bottom - 12;
    const int x = align == AlignLeft  ? 0 :
                  align == AlignRight ? (int)canvas_width(canvas) - width :
                                        ((int)canvas_width(canvas) - width) / 2;
    canvas_draw_box(canvas, x, top, width, 12);
    for(int i = 0; i < 3; i++) {
        if(align != AlignLeft) canvas_draw_line(canvas, x - 1 - i, top + i, x - 1 - i, bottom);
        if(align != AlignRight)
            canvas_draw_line(canvas, x + width + i, top + i, x + width + i, bottom);
    }
    canvas_invert_color(canvas);
    const int icon_x = align == AlignRight ? x + width - padding - icon_width : x + padding;
    const int text_x = align == AlignRight ? x + padding : x + padding + icon_width + 3;
    canvas_draw_xbm(canvas, icon_x, bottom - 10, icon_width, 7, icon);
    canvas_draw_str(canvas, text_x, bottom - 3, text);
    canvas_invert_color(canvas);
}

/* Sample a circle through the outer pixels' half-pixel boundary. Using r*r
 * alone leaves single-pixel tips; mixing that fill with midpoint outlines
 * also leaves bumps around diagonals. Share this contour for fills and rims. */
static int circle_extent(int radius, int row) {
    if(row > radius) return -1;
    int extent = radius;
    const int limit = radius * (radius + 1);
    while(extent * extent + row * row > limit)
        extent--;
    return extent;
}

static void draw_circle_outline(Canvas* canvas, int x, int y, int radius) {
    for(int row = -radius; row <= radius; row++) {
        const int offset = row < 0 ? -row : row;
        const int outer = circle_extent(radius, offset);
        /* Remove the four-neighbor interior of the same filled shape. This
         * keeps the one-pixel outline connected where the row width changes. */
        int inner = circle_extent(radius, offset + 1);
        if(inner >= outer) inner = outer - 1;
        if(inner < 0) {
            canvas_draw_line(canvas, x - outer, y + row, x + outer, y + row);
        } else {
            canvas_draw_line(canvas, x - outer, y + row, x - inner - 1, y + row);
            canvas_draw_line(canvas, x + inner + 1, y + row, x + outer, y + row);
        }
    }
}

static void draw_hit(Canvas* canvas, bool right, bool rim) {
    const int radius = rim ? DRUM_OUTER_RADIUS : DRUM_INNER_RADIUS;
    const int inner_radius = rim ? DRUM_INNER_RADIUS : 0;
    for(int y = -radius; y <= radius; y++) {
        const int offset = y < 0 ? -y : y;
        const int end = circle_extent(radius, offset);
        int start = 1;
        if(rim && offset <= inner_radius) start = circle_extent(inner_radius, offset) + 1;
        if(start > end) continue;
        int x1 = right ? DRUM_X + start : DRUM_X - end;
        int x2 = right ? DRUM_X + end : DRUM_X - start;
        canvas_draw_line(canvas, x1, DRUM_Y + y, x2, DRUM_Y + y);
    }
}

/* Clip each row explicitly so partially visible notes never wrap around the
 * canvas edge or draw into the title and status areas. */
static void lane_disc(Canvas* canvas, int x, int radius, Color color) {
    canvas_set_color(canvas, color);
    for(int y = -radius; y <= radius; y++) {
        const int extent = circle_extent(radius, y < 0 ? -y : y);
        int left = x - extent;
        int right = x + extent;
        if(right < 0 || left > 127) continue;
        if(left < 0) left = 0;
        if(right > 127) right = 127;
        canvas_draw_line(canvas, left, LANE_Y + y, right, LANE_Y + y);
    }
}

/* A narrow neck opens into a rounded bulb, following the incoming side. */
static void draw_balloon_tail(Canvas* canvas, int x, int direction, int radius) {
    static const uint8_t extent[] = {2, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5, 5, 5, 5, 5, 4, 4, 3, 2, 1};
    if(!direction) direction = 1;
    for(int pass = 0; pass < 3; pass++) {
        canvas_set_color(canvas, pass == 1 ? ColorBlack : ColorWhite);
        for(int i = pass; i < (int)sizeof(extent) - pass; i++) {
            int height = extent[i];
            for(int j = i - pass; j <= i + pass; j++)
                if(extent[j] < height) height = extent[j];
            height -= pass;
            if(height < 0) continue;
            int column = x + direction * (radius - 1 + i);
            if(column < 0 || column > 127) continue;
            canvas_draw_line(canvas, column, LANE_Y - height, column, LANE_Y + height);
        }
    }
}

static void draw_balloon_head_layer(Canvas* canvas, int x, bool hit, int layer) {
    const int radius = TAIKO_LANE_SMALL_RADIUS + (hit ? 1 : 0);
    const Color color = layer == 1 ? ColorBlack : ColorWhite;
    if(!hit || layer != 2) {
        lane_disc(canvas, x, radius - layer, color);
        return;
    }
    /* Inset the actual black contour by one pixel instead of drawing a smaller
     * circle, which leaves a heavy two-pixel border around the diagonals. */
    canvas_set_color(canvas, color);
    const int rim = radius - 1;
    for(int dx = -rim; dx <= rim; dx++) {
        const int offset = dx < 0 ? -dx : dx;
        int height = circle_extent(rim, offset) - 1;
        const int adjacent = circle_extent(rim, offset + 1);
        if(height > adjacent) height = adjacent;
        const int column = x + dx;
        if(height < 0 || column < 0 || column > 127) continue;
        canvas_draw_line(canvas, column, LANE_Y - height, column, LANE_Y + height);
    }
}

static void draw_balloon_head(Canvas* canvas, int x, bool hit) {
    for(int layer = 0; layer < 3; layer++)
        draw_balloon_head_layer(canvas, x, hit, layer);
}

static void draw_balloon_note(Canvas* canvas, const TaikoLaneRoll* note) {
    int direction = note->tail_x < note->x ? -1 : 1;
    draw_balloon_tail(canvas, note->x, direction, TAIKO_LANE_SMALL_RADIUS);
    draw_balloon_head(canvas, note->x, false);
}

static void balloon_neck_profile(uint8_t* profile, int length, int start, int end) {
    const int scale = length * length * length;
    for(int x = 0; x <= length; x++) {
        const int curve = x * x * (3 * length - 2 * x);
        profile[x] = start + ((end - start) * curve + scale / 2) / scale;
    }
    /* Before the inflection, shorten rather than lengthen successive stairs.
     * Swap a one-pixel run followed by a longer run, moving only their shared
     * corner one pixel. Mirror the adjustment on the other half of the cubic. */
    for(int x = 1; x + 2 <= length / 2; x++) {
        if(profile[x] == profile[x - 1] + 1 && profile[x + 1] == profile[x] + 1 &&
           profile[x + 2] == profile[x + 1]) {
            profile[x + 1]--;
            profile[length - x - 1]++;
        }
    }
}

static int balloon_bulb_extent(int radius, int column) {
    /* Pixel-art quarter circles, radii 6..19. Balance the short/long stair
     * runs in the shallow octant and mirror them into the steep octant. */
    static const uint8_t profiles[][BALLOON_MAX_RADIUS + 2] = {
        {6, 6, 6, 5, 5, 4, 2},
        {7, 7, 7, 6, 6, 5, 4, 2},
        {8, 8, 8, 7, 7, 6, 6, 4, 2},
        {9, 9, 9, 8, 8, 8, 7, 6, 5, 2},
        {10, 10, 10, 10, 9, 9, 9, 8, 7, 6, 3},
        {11, 11, 11, 10, 10, 10, 9, 9, 8, 7, 5, 2},
        {12, 12, 12, 11, 11, 11, 10, 10, 9, 8, 7, 5, 2},
        {13, 13, 13, 13, 12, 12, 12, 11, 11, 10, 9, 8, 6, 3},
        {14, 14, 14, 14, 13, 13, 13, 12, 12, 11, 10, 9, 8, 6, 3},
        {15, 15, 15, 15, 14, 14, 14, 13, 13, 12, 11, 10, 9, 8, 6, 3},
        {16, 16, 16, 16, 15, 15, 15, 14, 14, 13, 13, 12, 11, 10, 8, 6, 3},
        {17, 17, 17, 17, 17, 16, 16, 16, 15, 15, 14, 13, 12, 11, 10, 9, 7, 4},
        {18, 18, 18, 18, 17, 17, 17, 16, 16, 16, 15, 15, 14, 13, 12, 11, 9, 6, 3},
        {19, 19, 19, 19, 18, 18, 18, 17, 17, 17, 16, 16, 15, 14, 13, 12, 11, 9, 6, 3}};
    if(radius >= 6 && radius <= BALLOON_MAX_RADIUS + 1 && column <= radius)
        return profiles[radius - 6][column];
    return circle_extent(radius, column);
}

static int balloon_tail_inset(const uint8_t* profile, int count, int column, int inset) {
    int height = profile[column] - inset;
    /* Derive both border layers from one contour, instead of rounding three
     * separate curves. Four-neighbor insets keep diagonal strokes even. */
    for(int offset = 1; offset <= inset; offset++) {
        int left = column >= offset ? column - offset : 0;
        int right = column + offset;
        int adjacent = profile[left] - (inset - offset);
        if(height > adjacent) height = adjacent;
        adjacent = right < count ? profile[right] - (inset - offset) : -1;
        if(height > adjacent) height = adjacent;
    }
    return height;
}

static void draw_balloon_counter(Canvas* canvas, const TaikoPlayerView* view) {
    if(view->balloon.remaining && view->balloon.total) {
        char count[12], total[12];
        snprintf(count, sizeof(count), "%lu", view->balloon.remaining);
        snprintf(total, sizeof(total), "%lu", view->balloon.total);
        /* Size depends on the original count, so crossing a digit boundary never
         * shrinks the tail. The bulb and neck inflate; the head grows once. */
        const int inflation =
            (view->balloon.total - view->balloon.remaining) * 6 / view->balloon.total;
        int radius = canvas_string_width(canvas, total) / 2 + 2;
        if(radius < 7) radius = 7;
        radius += inflation;
        if(radius > BALLOON_MAX_RADIUS) radius = BALLOON_MAX_RADIUS;
        int direction = view->balloon.direction < 0 ? -1 : 1;
        const int reach = TAIKO_LANE_SMALL_RADIUS + 4 + 2 * radius + 1;
        /* The counter must stay readable even for a reverse-scroll balloon. */
        if(TAIKO_LANE_HIT_X + direction * reach < 0 || TAIKO_LANE_HIT_X + direction * reach > 127)
            direction = -direction;
        const int center = TAIKO_LANE_HIT_X + direction * (TAIKO_LANE_SMALL_RADIUS + 4 + radius);
        const int neck_start = TAIKO_LANE_HIT_X + direction * (TAIKO_LANE_SMALL_RADIUS - 1);
        const int neck_length = direction * (center - neck_start);
        int neck_height = 2 + inflation / 2;
        /* Keep the swelling neck tucked into the smaller head's edge. */
        if(neck_height > TAIKO_LANE_SMALL_RADIUS - 1) neck_height = TAIKO_LANE_SMALL_RADIUS - 1;
        uint8_t tail[BALLOON_MAX_RADIUS * 2 + 7];
        balloon_neck_profile(tail, neck_length, neck_height, radius + 1);
        const int tail_count = neck_length + radius + 2;
        for(int step = neck_length; step < tail_count; step++)
            tail[step] = balloon_bulb_extent(radius + 1, step - neck_length);
        const bool hit = view->balloon.remaining < view->balloon.total;
        for(int pass = 0; pass < 3; pass++) {
            /* Paint head and tail together in each layer, leaving the joining
             * white interior open rather than outlining a separate head. */
            draw_balloon_head_layer(canvas, TAIKO_LANE_HIT_X, hit, pass);
            /* A cubic taper meets the bulb at its widest column with a flat
             * tangent at both ends. Draw only the bulb's far half, so no
             * separate circle edge can protrude through the curved neck. */
            for(int step = 0; step < tail_count; step++) {
                const int height = balloon_tail_inset(tail, tail_count, step, pass);
                if(height < 0) continue;
                int x = neck_start + direction * step;
                canvas_draw_line(canvas, x, LANE_Y - height, x, LANE_Y + height);
            }
        }
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(
            canvas, center - canvas_string_width(canvas, count) / 2, LANE_Y + 3, count);
        /* Close only the thin black ring over the joined fill; do not repaint
         * a white halo through the neck. */
        if(hit) draw_circle_outline(canvas, TAIKO_LANE_HIT_X, LANE_Y, TAIKO_LANE_SMALL_RADIUS);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void draw_gogo_background(Canvas* canvas, const TaikoPlayerView* view) {
    if(!view->gogo) return;
    static const uint8_t bayer[4][4] = {
        {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    static const uint8_t density[] = {12, 8, 4, 2};
    static const uint8_t jitter[] = {0, 0, 1, 0, 0, 0, 1, 0};
    static const int8_t pulse[] = {0, 0, 1, 0, 0, -1, 0, 0};
    /* Source time keeps the animation still on pause and deterministic on seek.
     * Four dithered rows ripple gently, with a small brightness variation.
     * Keep the effect within five rows and underneath the notes and target. */
    const uint32_t phase = view->position_ms / 70;
    const int brightness = pulse[phase & 7];
    canvas_set_color(canvas, ColorWhite);
    for(int x = 0; x < 128; x++) {
        const int offset = jitter[((x / 4) + phase) & 7];
        for(size_t row = 0; row < COUNT_OF(density); row++) {
            const int y = LANE_TOP + row + offset;
            const int threshold = density[row] + (row ? brightness : 0);
            if(bayer[(row + phase / 2) & 3][(x + phase) & 3] < threshold)
                canvas_draw_dot(canvas, x, y);
        }
    }
    canvas_set_color(canvas, ColorBlack);
}

static void draw_target_flash(Canvas* canvas) {
    /* Row extents of the canvas midpoint circle at radius nine. Unlike flooring
     * the circle equation, this gives the left/right tips five-pixel arcs. */
    static const uint8_t outer_extent[] = {9, 9, 9, 8, 8, 7, 7, 6, 4, 2};
    canvas_set_color(canvas, ColorWhite);
    /* Add a continuous band outside radius seven, preserving every idle pixel
     * inside it, including the black gap and any overlapping notes. */
    for(int y = -9; y <= 9; y++) {
        const int outer = outer_extent[y < 0 ? -y : y];
        int inner = 0;
        while(inner <= outer && inner * inner + y * y < 7 * 7)
            inner++;
        if(inner > outer) continue;
        canvas_draw_line(
            canvas, TAIKO_LANE_HIT_X - outer, LANE_Y + y, TAIKO_LANE_HIT_X - inner, LANE_Y + y);
        canvas_draw_line(
            canvas, TAIKO_LANE_HIT_X + inner, LANE_Y + y, TAIKO_LANE_HIT_X + outer, LANE_Y + y);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void draw_lane(Canvas* canvas, const TaikoPlayerView* view) {
    canvas_draw_box(canvas, 0, LANE_TOP, 128, LANE_BOTTOM - LANE_TOP + 1);
    draw_gogo_background(canvas, view);
    canvas_set_color(canvas, ColorWhite);
    for(unsigned x = 0; x < 128; x++) {
        if(view->bar_columns[x / 8] & (1U << (x % 8)))
            canvas_draw_line(canvas, x, LANE_TOP, x, LANE_BOTTOM);
    }
    /* Keep the hollow target clear of barlines and the bright GOGO band. */
    lane_disc(canvas, TAIKO_LANE_HIT_X, view->gogo ? 8 : 7, ColorBlack);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_circle(canvas, TAIKO_LANE_HIT_X, LANE_Y, 7);
    canvas_draw_circle(canvas, TAIKO_LANE_HIT_X, LANE_Y, 5);
    for(size_t i = view->roll_count; i > 0; i--) {
        TaikoLaneRoll roll = view->lane_rolls[i - 1];
        if(roll.kind == 7 || roll.kind == 9) {
            draw_balloon_note(canvas, &roll);
            continue;
        }
        int radius = roll.kind == 5 ? TAIKO_LANE_SMALL_RADIUS : TAIKO_LANE_BIG_RADIUS;
        int left = roll.x < roll.tail_x ? roll.x : roll.tail_x;
        int right = roll.x > roll.tail_x ? roll.x : roll.tail_x;
        /* White center with a black outline and a one-pixel white outer halo. */
        for(int pass = 0; pass < 3; pass++) {
            int r = radius - pass;
            canvas_set_color(canvas, pass == 1 ? ColorBlack : ColorWhite);
            for(int y = -r; y <= r; y++) {
                const int extent = circle_extent(r, y < 0 ? -y : y);
                int x1 = left - extent, x2 = right + extent;
                if(x2 < 0 || x1 > 127) continue;
                if(x1 < 0) x1 = 0;
                if(x2 > 127) x2 = 127;
                canvas_draw_line(canvas, x1, LANE_Y + y, x2, LANE_Y + y);
            }
        }
    }
    for(size_t i = view->lane_count; i > 0; i--) {
        TaikoLaneNote note = view->lane_notes[i - 1];
        int radius = note.kind >= 3 ? TAIKO_LANE_BIG_RADIUS : TAIKO_LANE_SMALL_RADIUS;
        if(view->gogo) lane_disc(canvas, note.x, radius + 1, ColorBlack);
        lane_disc(canvas, note.x, radius, ColorWhite);
        /* Thicken large-note borders inward without changing the outer radius. */
        lane_disc(canvas, note.x, radius - (note.kind == 3 ? 2 : 1), ColorBlack);
        if(note.kind == 2 || note.kind == 4)
            lane_disc(canvas, note.x, radius - (note.kind == 4 ? 3 : 2), ColorWhite);
    }
    canvas_set_color(canvas, ColorBlack);
}

void taiko_player_view_draw(Canvas* canvas, const TaikoPlayerView* view, FuriString* text) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    if(view->menu.open) {
        draw_menu(canvas, &view->menu);
        return;
    }
    furi_string_set(text, view->title);
    elements_string_fit_width(canvas, text, 124);
    canvas_draw_str(canvas, 2, 9, furi_string_get_cstr(text));

    uint32_t now = furi_get_tick();
    const bool hit_active = view->hit_until && (int32_t)(view->hit_until - now) > 0;
    draw_lane(canvas, view);
    if(view->target_hit_until && (int32_t)(view->target_hit_until - now) > 0)
        draw_target_flash(canvas);
    if(hit_active) {
        draw_hit(canvas, view->hit_right, view->hit_rim);
    }
    draw_circle_outline(canvas, DRUM_X, DRUM_Y, DRUM_INNER_RADIUS);
    draw_circle_outline(canvas, DRUM_X, DRUM_Y, DRUM_OUTER_RADIUS);
    canvas_draw_line(
        canvas, DRUM_X, DRUM_Y - DRUM_OUTER_RADIUS, DRUM_X, DRUM_Y + DRUM_OUTER_RADIUS);

    char status[24];
    if(!view->bpm_tenths)
        snprintf(status, sizeof(status), "BPM: --");
    else if(view->bpm_tenths % 10)
        snprintf(
            status, sizeof(status), "BPM: %lu.%lu", view->bpm_tenths / 10, view->bpm_tenths % 10);
    else
        snprintf(status, sizeof(status), "BPM: %lu", view->bpm_tenths / 10);
    furi_string_set(text, status);
    elements_string_fit_width(canvas, text, 49);
    canvas_draw_str(canvas, 38, 39, furi_string_get_cstr(text));
    unsigned speed = taiko_menu_scroll_tenths(&view->menu);
    snprintf(status, sizeof(status), "Speed: %u.%u", speed / 10, speed % 10);
    canvas_draw_str(canvas, 38, 49, status);

    char time[16];
    uint32_t seconds = view->position_ms / 1000;
    snprintf(time, sizeof(time), "%lu:%02lu", seconds / 60, seconds % 60);
    /* Keep Normal's current center as the shared axis for both status lines. */
    const int status_center = 89 + canvas_string_width(canvas, "Normal") / 2;
    canvas_draw_str(canvas, status_center - canvas_string_width(canvas, time) / 2, 39, time);
    furi_string_set(text, view->course);
    elements_string_fit_width(canvas, text, 31);
    const char* course = furi_string_get_cstr(text);
    canvas_draw_str(canvas, status_center - canvas_string_width(canvas, course) / 2, 49, course);
    canvas_draw_frame(canvas, 123, 30, 5, 21);
    const unsigned volume = MIN(view->volume, TAIKO_MASTER_VOLUME_MAX);
    const int volume_height =
        (volume * 19 + TAIKO_MASTER_VOLUME_MAX / 2) / TAIKO_MASTER_VOLUME_MAX;
    if(volume_height) canvas_draw_box(canvas, 124, 50 - volume_height, 3, volume_height);
    draw_control(canvas, "BWD", AlignLeft);
    draw_control(
        canvas,
        view->loading                  ? "..." :
        view->paused || view->finished ? "Play" :
                                         "Pause",
        AlignCenter);
    draw_control(canvas, "FWD", AlignRight);
    /* The enlarged balloon is a popup above the lane and drum indicator. */
    draw_balloon_counter(canvas, view);
}
