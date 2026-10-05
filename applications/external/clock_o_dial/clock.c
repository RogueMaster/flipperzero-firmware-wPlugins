#include <math.h>

#include "clock.h"
#include "clock_o_dial_icons.h"

#ifndef M_TWOPI
#define M_TWOPI (2.0 * M_PI)
#endif
#define M_TWOPI_F ((float)(2.0 * M_PI))

#define OFS_LEFT_X  31
#define OFS_MID_X   63
#define OFS_RIGHT_X 96
#define OFS_Y       31

#define M_RAD   26
#define HMS_OFS 8

#define FACE_RADIUS 31

void draw_line(Canvas* c, uint8_t ofs_x, Line* l, LineType type) { // Bresenham-Algorithm
    int8_t x = l->start.x, y = l->start.y;
    int8_t dx = abs(l->end.x - x), sx = x < l->end.x ? 1 : -1;
    int8_t dy = -abs(l->end.y - y), sy = y < l->end.y ? 1 : -1;
    int8_t error = dx + dy, e2;
    while(true) {
        if(type == Thick) {
            canvas_draw_disc(c, ofs_x + x, OFS_Y - y, 1);
        } else {
            canvas_draw_dot(c, ofs_x + x, OFS_Y - y);
        }
        if((x == l->end.x) && (y == l->end.y)) break;
        e2 = 2 * error;
        if(e2 > dy) x += sx, error += dy;
        if(e2 < dx) y += sy, error += dx;
    }
}

void set_point(Point* p, float ang, float radius) {
    p->x = (int8_t)round(sin((double)ang) * (double)radius);
    p->y = (int8_t)round(cos((double)ang) * (double)radius);
}

void set_line(Line* l, float ang, float start_rad, float end_rad) {
    set_point(&l->start, ang, start_rad);
    set_point(&l->end, ang, end_rad);
}

void draw_hand(Canvas* canvas, uint8_t ofs_x, float ang, int radius, bool thick) {
    Line l;
    set_line(&l, ang, thick ? HMS_OFS : -HMS_OFS, radius);
    draw_line(canvas, ofs_x, &l, thick ? Thick : Normal);
    if(thick) {
        // Draw thin line from end back to center
        l.end.x = 0, l.end.y = 0;
        draw_line(canvas, ofs_x, &l, Normal);
        // Draw thin line at the end of the hand (extending just a few pixels beyond the bold part)
        set_line(&l, ang, radius, radius + 3);
        draw_line(canvas, ofs_x, &l, Normal);
    }
}

void calc_clock_face(ClockFace* face) {
    // Precompute the worked-pattern's dither pixels and their dial angle once, so draw_timer
    // only has to check arc containment (no atan2f) on every one-second redraw.
    uint16_t fill_index = 0;
    for(int8_t y = -FILL_HALF_SIZE; y <= FILL_HALF_SIZE; y++) {
        for(int8_t x = -FILL_HALF_SIZE; x <= FILL_HALF_SIZE; x++) {
            if(((x + y) & 1) != 0) continue; // dense 45-degree checkerboard (half the pixels)
            float angle = atan2f((float)x, (float)y);
            if(angle < 0.0f) angle += M_TWOPI_F;
            face->fill_pixels[fill_index].x = x;
            face->fill_pixels[fill_index].y = y;
            face->fill_pixels[fill_index].angle = angle;
            fill_index++;
        }
    }
    face->fill_pixel_count = fill_index;
}

// Angle (radians, clockwise from top) for a point in time within a repeating 12-hour dial.
static float wallclock_angle(uint32_t seconds_of_day) {
    return fmodf((float)seconds_of_day, 12.0f * 3600.0f) / (12.0f * 3600.0f) * M_TWOPI_F;
}

// True if the forward sweep of `length` radians starting at `start` (clockwise) covers `angle`.
// A length >= 2*PI (an arc that has lapped the dial) covers every angle.
static bool angle_in_forward_arc(float angle, float start, float length) {
    if(length <= 0.0f) return false;
    float delta = fmodf(angle - start, M_TWOPI_F);
    if(delta < 0.0f) delta += M_TWOPI_F;
    return delta < length;
}

// Forward duration in seconds from `from` to `to`, both seconds-of-day, wrapping past midnight.
static uint32_t wallclock_span(uint32_t from, uint32_t to) {
    return to >= from ? to - from : to + 86400 - from;
}

void draw_timer(
    Canvas* canvas,
    ClockFace* face,
    uint8_t timer_duration_hours,
    uint32_t elapsed_seconds,
    uint16_t ms,
    bool running,
    bool has_been_started,
    uint32_t now_wallclock_secs,
    uint32_t animation_wallclock_secs,
    uint32_t start_wallclock_secs,
    const BreakLog* break_log,
    const UiOverlay* ui) {
    // Draw square 12-hour clock face on left side, real time always
    static char time_buf[10];

    uint32_t timer_duration_seconds = timer_duration_hours * 3600;
    uint32_t timer_duration_ms = timer_duration_seconds * 1000;

    // The hand always shows the real current time - one hand, no exceptions.
    float hand_angle = wallclock_angle(now_wallclock_secs);

    if(has_been_started) {
        // History span: everything from shift start to now, work and breaks both. Each logged
        // break gets carved out of it individually below, rather than showing one lumped gap.
        float history_start_angle = wallclock_angle(start_wallclock_secs);
        float history_length = (float)wallclock_span(start_wallclock_secs, now_wallclock_secs) /
                               (12.0f * 3600.0f) * M_TWOPI_F;

        // Predicted segment: from now, for however much work remains at the current pace.
        // While on a break this starts sliding forward with real time, which pushes the
        // predicted finish time forward too.
        uint32_t remaining_seconds = timer_duration_seconds > elapsed_seconds ?
                                         timer_duration_seconds - elapsed_seconds :
                                         0;
        float predicted_start_angle = hand_angle;
        float predicted_length = (float)remaining_seconds / (12.0f * 3600.0f) * M_TWOPI_F;

        // Precompute every break's angle/length once per frame rather than per pixel
        float break_start_angles[MAX_BREAKS + 1];
        float break_lengths[MAX_BREAKS + 1];
        uint8_t break_angle_count = 0;
        for(uint8_t i = 0; i < break_log->count && break_angle_count < MAX_BREAKS + 1; i++) {
            break_start_angles[break_angle_count] =
                wallclock_angle(break_log->items[i].start_wallclock_secs);
            break_lengths[break_angle_count] = (float)wallclock_span(
                                                   break_log->items[i].start_wallclock_secs,
                                                   break_log->items[i].end_wallclock_secs) /
                                               (12.0f * 3600.0f) * M_TWOPI_F;
            break_angle_count++;
        }
        if(break_log->live_active && break_angle_count < MAX_BREAKS + 1) {
            break_start_angles[break_angle_count] =
                wallclock_angle(break_log->live_start_wallclock_secs);
            break_lengths[break_angle_count] =
                (float)wallclock_span(break_log->live_start_wallclock_secs, now_wallclock_secs) /
                (12.0f * 3600.0f) * M_TWOPI_F;
            break_angle_count++;
        }

        // Each candidate pixel's position and dial angle were precomputed once at startup -
        // only its arc containment (cheap fmodf-based checks) needs doing every redraw.
        for(uint16_t i = 0; i < face->fill_pixel_count; i++) {
            int8_t x = face->fill_pixels[i].x;
            int8_t y = face->fill_pixels[i].y;
            float pixel_angle = face->fill_pixels[i].angle;

            if(angle_in_forward_arc(pixel_angle, history_start_angle, history_length)) {
                bool in_break = false;
                for(uint8_t bi = 0; bi < break_angle_count; bi++) {
                    if(angle_in_forward_arc(
                           pixel_angle, break_start_angles[bi], break_lengths[bi])) {
                        in_break = true;
                        break;
                    }
                }
                if(!in_break) canvas_draw_dot(canvas, OFS_LEFT_X + x, OFS_Y - y);
            } else if(angle_in_forward_arc(pixel_angle, predicted_start_angle, predicted_length)) {
                // Predicted pattern: sparse grid dither (a quarter of the pixels) - lets more
                // light through than the worked pattern, since this time hasn't happened yet.
                bool predicted_dither = ((x & 1) == 0) && ((y & 1) == 0);
                if(predicted_dither) canvas_draw_dot(canvas, OFS_LEFT_X + x, OFS_Y - y);
            }
        }
    }

    // Main draw calls: hand, disc, frame, etc.
    draw_hand(canvas, OFS_LEFT_X, hand_angle, M_RAD, true);
    canvas_draw_disc(canvas, OFS_LEFT_X, OFS_Y, 2);
    canvas_set_bitmap_mode(canvas, true);
    canvas_draw_icon(canvas, 0, 0, &I_frame);
    canvas_set_bitmap_mode(canvas, false);

    // Draw digital timer and status on right side
    uint32_t total_elapsed_ms = elapsed_seconds * 1000 + ms;
    uint32_t remaining_ms =
        timer_duration_ms > total_elapsed_ms ? timer_duration_ms - total_elapsed_ms : 0;

    // Convert to hours, minutes, and (in long format) seconds
    uint32_t remaining_seconds = remaining_ms / 1000;
    uint8_t hours = remaining_seconds / 3600;
    uint8_t minutes = (remaining_seconds % 3600) / 60;

    // Format time string. Once eco mode has slowed redraws to once a minute, seconds aren't
    // actually being tracked live anymore, so show placeholder dashes instead of a stale number.
    // In Set mode nothing is ticking in the first place - the configured duration always has
    // exact, correct (":00") seconds, so there's never anything stale to dash out there.
    if(ui->long_time_format) {
        if(ui->animations_frozen && has_been_started) {
            snprintf(time_buf, 10, "%u:%02u:--", hours, minutes);
        } else {
            uint8_t seconds = remaining_seconds % 60;
            snprintf(time_buf, 10, "%u:%02u:%02u", hours, minutes, seconds);
        }
    } else {
        snprintf(time_buf, 10, "%u:%02u", hours, minutes);
    }

    // Determine status
    bool is_finished = total_elapsed_ms >= timer_duration_ms;
    bool is_working = running && !is_finished;
    bool is_break = has_been_started && !running && !is_finished;
    const char* status;
    if(!has_been_started) {
        status = "Set shift";
    } else if(is_finished) {
        status = "Finished";
    } else if(running) {
        status = "Working";
    } else {
        status = "Break";
    }

    if(!has_been_started) {
        // Set Shift layout: label on top, time near the bottom - left/right arrows (shift
        // length) and up/down arrows (time format) surround the time readout on all four sides.
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, OFS_RIGHT_X, 17, AlignCenter, AlignCenter, status);

        int32_t time_y = 40;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, OFS_RIGHT_X, time_y, AlignCenter, AlignCenter, time_buf);

        // Each format's reference width is measured once and cached, since the format itself
        // can change at runtime (unlike the shift-length digits, which never change the format).
        static uint16_t half_w_short = 0;
        static uint16_t half_w_long = 0;
        uint16_t half_w;
        if(ui->long_time_format) {
            if(half_w_long == 0) half_w_long = canvas_string_width(canvas, "12:00:00") / 2;
            half_w = half_w_long;
        } else {
            if(half_w_short == 0) half_w_short = canvas_string_width(canvas, "12:00") / 2;
            half_w = half_w_short;
        }
        int32_t left_x = OFS_RIGHT_X - half_w - 6;
        int32_t right_x = OFS_RIGHT_X + half_w + 2;
        int32_t lr_arrow_y = time_y - 3;
        canvas_draw_icon(canvas, left_x, lr_arrow_y, &I_arrow_left);
        canvas_draw_icon(canvas, right_x, lr_arrow_y, &I_arrow_right);

        // Up/down sit close to the text like left/right do, measured off the font's actual
        // height instead of a guessed offset - moved up and given a bit more breathing room so
        // the CLOSING hold-overlay (which starts at y=44) doesn't cover the time readout.
        int32_t half_h = canvas_get_font_params(canvas, FontPrimary)->height / 2;
        int32_t up_arrow_y = time_y - half_h - 3 - icon_get_height(&I_arrow_up);
        int32_t down_arrow_y = time_y + half_h + 3;
        canvas_draw_icon(
            canvas, OFS_RIGHT_X - icon_get_width(&I_arrow_up) / 2, up_arrow_y, &I_arrow_up);
        canvas_draw_icon(
            canvas, OFS_RIGHT_X - icon_get_width(&I_arrow_down) / 2, down_arrow_y, &I_arrow_down);
    } else {
        // Working/Break/Finished layout: time on top, status below it (unchanged).
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, OFS_RIGHT_X, OFS_Y - 5, AlignCenter, AlignCenter, time_buf);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, OFS_RIGHT_X, OFS_Y + 8, AlignCenter, AlignCenter, status);
    }

    // Everything on this bottom row - the croc tail and the working/break icon - shares the
    // screen's own bottom edge regardless of each sprite's own height.
    int32_t icon_bottom_y = 64;

    // Alternate frames once a second - draw callback already ticks at ~1Hz, so no separate
    // animation timer is needed. Uses real time even when the dial itself is frozen (a finished
    // shift), so the mascot still animates while the hand stays put. Once eco mode freezes things
    // after a minute of inactivity, that 1Hz alternation stops - instead the frame blips to 2 for
    // one second right as each real-time minute ticks over, then holds frame 1 until the next.
    bool show_frame_2 = ui->animations_frozen ? ui->eco_blip_active :
                                                 (animation_wallclock_secs % 2 != 0);

    // The croc's tail tip, flush against the face's own right edge with no gap - always visible.
    // Asleep whenever it isn't actively working (Set mode, on a break, or shift finished); awake
    // and animating only while working. Both states animate at the same 1Hz (eco-frozen to 1/min
    // when idle).
    bool is_sleeping = !is_working;
    const Icon* croc_icon;
    if(is_sleeping) {
        croc_icon = show_frame_2 ? &I_sleep_2 : &I_sleep_1;
    } else {
        croc_icon = show_frame_2 ? &I_croc_2 : &I_croc_1;
    }
    int32_t croc_x = icon_get_width(&I_frame);
    canvas_draw_icon(canvas, croc_x, icon_bottom_y - icon_get_height(croc_icon), croc_icon);

    const Icon* status_icon = NULL;
    if(is_working) {
        status_icon = show_frame_2 ? &I_working_2 : &I_working_1;
    } else if(is_break) {
        status_icon = show_frame_2 ? &I_coffee_2 : &I_coffee_1;
    }
    if(status_icon != NULL) {
        // 4px gap to the right of whichever croc/sleep sprite is currently showing - measured
        // off its actual width so the gap stays consistent even though the sleeping sprite is
        // narrower than the awake one.
        int32_t status_icon_x = croc_x + icon_get_width(croc_icon) + 4;
        canvas_draw_icon(
            canvas, status_icon_x, icon_bottom_y - icon_get_height(status_icon), status_icon);
    }

    // Sound, eco, backlight, and vibro all share one top-right slot; only one of the four is
    // ever drawn at a time. Anchored to a shared center (derived from the widest/tallest of the
    // four) so swapping between them never shifts position.
    const Icon* top_right_icon = NULL;
    if(ui->top_right_icon_slot == TopRightIconSound) {
        top_right_icon = ui->sound_enabled ? &I_sound_on : &I_sound_off;
    } else if(ui->top_right_icon_slot == TopRightIconEco) {
        top_right_icon = ui->eco_mode_enabled ? &I_eco_on : &I_eco_off;
    } else if(ui->top_right_icon_slot == TopRightIconBacklight) {
        top_right_icon = ui->backlight_on ? &I_light_on : &I_light_off;
    } else if(ui->top_right_icon_slot == TopRightIconVibro) {
        top_right_icon = ui->vibro_enabled ? &I_vibro_on : &I_vibro_off;
    }
    if(top_right_icon != NULL) {
        int32_t ref_w = icon_get_width(&I_sound_on);
        if(icon_get_width(&I_eco_on) > ref_w) ref_w = icon_get_width(&I_eco_on);
        if(icon_get_width(&I_light_on) > ref_w) ref_w = icon_get_width(&I_light_on);
        if(icon_get_width(&I_vibro_on) > ref_w) ref_w = icon_get_width(&I_vibro_on);
        int32_t ref_h = icon_get_height(&I_sound_on);
        if(icon_get_height(&I_eco_on) > ref_h) ref_h = icon_get_height(&I_eco_on);
        if(icon_get_height(&I_light_on) > ref_h) ref_h = icon_get_height(&I_light_on);
        if(icon_get_height(&I_vibro_on) > ref_h) ref_h = icon_get_height(&I_vibro_on);
        // Use ceil(ref_w/2) so the widest icon's right edge lands exactly on the screen edge
        // rather than one pixel past it when ref_w is odd.
        int32_t center_x = 128 - (ref_w + 1) / 2 - 1;
        int32_t center_y = 2 + ref_h / 2 - 1;

        int32_t x = center_x - icon_get_width(top_right_icon) / 2;
        int32_t y = center_y - icon_get_height(top_right_icon) / 2;
        canvas_draw_icon(canvas, x, y, top_right_icon);
    }

    if(ui->hold_active) {
        float frac = ui->hold_fraction;
        if(frac < 0.0f) frac = 0.0f;
        if(frac > 1.0f) frac = 1.0f;
        float eased = 0.5f * (1.0f - cosf(frac * (float)M_PI)); // ease-in-out sine

        // Working/break (RESETTING, or CLOSING while working): reuse the status-text row and
        // stay tight below it, clear of the mascot animation at the bottom of the screen. Set
        // mode (INFO, or CLOSING there) instead reuses the "Set shift" title's row, with the bar
        // above it near the top edge, clear of both the time readout/arrows in the middle and
        // the sleep animation at the bottom. The label-to-bar gap is the same 7px (from the
        // label's center) in both layouts, just mirrored top-to-bottom.
        const int32_t bar_height = 2;
        // The box and the bar both start right past the face sprite's own right edge (never a
        // pixel sooner), so neither can ever cut into it - the box additionally needs no further
        // gap since it's just erasing, while the bar gets a few extra px of breathing room.
        const int32_t frame_right = icon_get_width(&I_frame);
        const int32_t bar_margin = 4;
        int32_t label_y, bar_y, box_y0, box_y1;
        if(has_been_started) {
            label_y = OFS_Y + 8; // same row the "Working"/"Break" status text uses
            bar_y = label_y + 7;
            box_y0 = label_y - 6;
            box_y1 = bar_y + bar_height + 1;
        } else {
            label_y = 17; // same row the "Set shift" title uses
            bar_y = label_y - 7 - bar_height;
            box_y0 = bar_y - 2;
            box_y1 = label_y + 6;
        }
        int32_t bar_x0 = frame_right + bar_margin;
        int32_t bar_x1 = 128 - bar_margin;
        int32_t bar_width = bar_x1 - bar_x0;

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, frame_right, box_y0, 128 - frame_right, box_y1 - box_y0);
        canvas_set_color(canvas, ColorBlack);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, OFS_RIGHT_X, label_y, AlignCenter, AlignCenter, ui->hold_label);
        canvas_draw_box(canvas, bar_x0, bar_y, (int32_t)(bar_width * eased), bar_height);
    } else if(ui->break_limit_message) {
        // Same idea as the hold overlay above: reuse the status-text row, so this sits between
        // the time readout and the mascot animation, and never cuts into the face sprite's edge.
        const int32_t frame_right = icon_get_width(&I_frame);
        // A couple px lower than the status text's own row and with tighter padding, so the
        // block clears the time readout above it instead of just touching it.
        const int32_t label_y = OFS_Y + 10;
        const int32_t line_gap = 8;
        const int32_t line1_y = label_y - line_gap / 2;
        const int32_t line2_y = label_y + line_gap / 2;
        const int32_t box_y0 = line1_y - 4;
        const int32_t box_y1 = line2_y + 4;

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, frame_right, box_y0, 128 - frame_right, box_y1 - box_y0);
        canvas_set_color(canvas, ColorBlack);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, OFS_RIGHT_X, line1_y, AlignCenter, AlignCenter, "Too many");
        canvas_draw_str_aligned(canvas, OFS_RIGHT_X, line2_y, AlignCenter, AlignCenter, "breaks!");
    }
}

void init_timer_config(TimerConfig* cfg) {
    cfg->version = CONFIG_VERSION;
    cfg->timer_duration_hours = DEFAULT_TIMER_HOURS; // Default 8 hours
    cfg->sound_enabled = true; // Sound enabled by default
    cfg->backlight_on = true; // Backlight enforced on by default
    cfg->eco_mode_enabled = true; // Eco mode enabled by default
    cfg->vibro_enabled = true; // Vibro enabled by default
    cfg->long_time_format = false; // Short H:MM format by default
}

void modify_timer_up(TimerConfig* cfg) {
    if(cfg->timer_duration_hours < MAX_TIMER_HOURS) {
        cfg->timer_duration_hours++;
    }
}

void modify_timer_down(TimerConfig* cfg) {
    if(cfg->timer_duration_hours > 1) {
        cfg->timer_duration_hours--;
    }
}
