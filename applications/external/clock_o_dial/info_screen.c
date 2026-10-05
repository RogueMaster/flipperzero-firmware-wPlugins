#include "info_screen.h"
#include "clock_o_dial_icons.h"

// Small breathing room between the title and subtitle, in both draw_page_header and
// page_content_top below.
#define SUBTITLE_Y_NUDGE 1

// Shared page header: title + subtitle, packed flush against the top edge so each page keeps as
// much vertical room as possible for its own content below.
static void draw_page_header(Canvas* canvas, const char* title, const char* subtitle) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        64,
        canvas_get_font_params(canvas, FontPrimary)->height + SUBTITLE_Y_NUDGE,
        AlignCenter,
        AlignTop,
        subtitle);
}

// Y coordinate where a page's own content can start, just below the header.
static int32_t page_content_top(Canvas* canvas) {
    return canvas_get_font_params(canvas, FontPrimary)->height + SUBTITLE_Y_NUDGE +
           canvas_get_font_params(canvas, FontSecondary)->height;
}

// Distance from the d-pad's center to an arrow's center, given how thick the arrow is along its
// axis and how much of a gap should separate it from the opposing arrow.
static int32_t arrow_center_dist(int32_t arrow_size, int32_t cross_gap) {
    return (arrow_size + cross_gap + 1) / 2;
}

// Distance from the d-pad's center to a bound option icon's center, placed just past its arrow
// with a small gap between the two.
static int32_t
    icon_center_dist(int32_t arrow_d, int32_t arrow_size, int32_t icon_size, int32_t gap) {
    return arrow_d + (arrow_size + icon_size) / 2 + gap;
}

// Page 1: a quick-reference diagram of what each button does while a shift is active. The D-pad
// sits centered on the screen; each bound option's icon trails just past its arrow, in the
// direction of the physical button it's bound to - "up" maps to sound, "down" to backlight, etc.
static void draw_options_page(Canvas* canvas) {
    draw_page_header(canvas, "OPTIONS", "(while working)");

    const int32_t cross_gap = 2; // gap left between the two opposing arrows on each axis
    const int32_t icon_gap = 3; // gap between each arrow and its bound icon
    const int32_t lr_extra_d = 6; // left/right block pushed further out from center
    const int32_t lr_extra_y = 1; // left/right block nudged down
    const int32_t down_extra_y = 2; // down block nudged further down
    const int32_t whole_block_down = 1; // the entire d-pad diagram nudged down

    // Both axes' arrows are the same size along their axis (5px), so one shared radius works.
    const int32_t arrow_size = icon_get_height(&I_arrow_up);
    const int32_t arrow_d = arrow_center_dist(arrow_size, cross_gap);
    const int32_t lr_arrow_d = arrow_d + lr_extra_d;

    const int32_t sound_h = icon_get_height(&I_sound_on);
    const int32_t light_h = icon_get_height(&I_light_on);
    const int32_t vibro_w = icon_get_width(&I_vibro_on);
    const int32_t eco_w = icon_get_width(&I_eco_on);

    const int32_t sound_d = icon_center_dist(arrow_d, arrow_size, sound_h, icon_gap);
    const int32_t light_d = icon_center_dist(arrow_d, arrow_size, light_h, icon_gap);
    const int32_t vibro_d = icon_center_dist(arrow_d, arrow_size, vibro_w, icon_gap) + lr_extra_d;
    const int32_t eco_d = icon_center_dist(arrow_d, arrow_size, eco_w, icon_gap) + lr_extra_d;

    const int32_t cx = 64; // screen center
    const int32_t top = page_content_top(canvas);
    const int32_t cy = top + (64 - top) / 2 +
                       whole_block_down; // sound/light are the same size, so this is symmetric

    canvas_draw_icon(
        canvas, cx - icon_get_width(&I_arrow_up) / 2, cy - arrow_d - arrow_size / 2, &I_arrow_up);
    canvas_draw_icon(
        canvas,
        cx - icon_get_width(&I_arrow_down) / 2,
        cy + arrow_d - arrow_size / 2 + down_extra_y,
        &I_arrow_down);
    canvas_draw_icon(
        canvas,
        cx - lr_arrow_d - icon_get_width(&I_arrow_left) / 2,
        cy - icon_get_height(&I_arrow_left) / 2 + lr_extra_y,
        &I_arrow_left);
    canvas_draw_icon(
        canvas,
        cx + lr_arrow_d - icon_get_width(&I_arrow_right) / 2,
        cy - icon_get_height(&I_arrow_right) / 2 + lr_extra_y,
        &I_arrow_right);

    // Up -> sound, Down -> backlight, Left -> vibro, Right -> eco (matches the real bindings)
    canvas_draw_icon(
        canvas, cx - icon_get_width(&I_sound_on) / 2, cy - sound_d - sound_h / 2, &I_sound_on);
    canvas_draw_icon(
        canvas,
        cx - icon_get_width(&I_light_on) / 2,
        cy + light_d - light_h / 2 + down_extra_y,
        &I_light_on);
    canvas_draw_icon(
        canvas,
        cx - vibro_d - vibro_w / 2,
        cy - icon_get_height(&I_vibro_on) / 2 + lr_extra_y,
        &I_vibro_on);
    canvas_draw_icon(
        canvas,
        cx + eco_d - eco_w / 2,
        cy - icon_get_height(&I_eco_on) / 2 + lr_extra_y,
        &I_eco_on);
}

// The Canvas API has no built-in letter-spacing, so "on GitHub" is drawn one glyph at a time to
// open up a small gap between each letter.
static void
    draw_tracked_str(Canvas* canvas, int32_t x, int32_t y, int32_t letter_gap, const char* str) {
    char glyph[2] = {0, 0};
    int32_t cursor = x;
    for(const char* p = str; *p; p++) {
        glyph[0] = *p;
        canvas_draw_str_aligned(canvas, cursor, y, AlignLeft, AlignTop, glyph);
        cursor += canvas_glyph_width(canvas, (uint16_t)(unsigned char)*p) + letter_gap;
    }
}

// Total width draw_tracked_str would take up, so following content can be placed after it.
static int32_t tracked_str_width(Canvas* canvas, int32_t letter_gap, const char* str) {
    int32_t width = 0;
    bool first = true;
    for(const char* p = str; *p; p++) {
        if(!first) width += letter_gap;
        width += canvas_glyph_width(canvas, (uint16_t)(unsigned char)*p);
        first = false;
    }
    return width;
}

// Page 2: credits, with a QR code to the repo on the left and a short thank-you on the right -
// the same gap separates the QR code from the left page-nav arrow and from the text column, and
// the same margin separates it from the subtitle above and the screen edge below.
static void draw_about_page(Canvas* canvas) {
    draw_page_header(canvas, "DEVELOPED BY", "Ilia Petrov-Komotskii");

    const int32_t side_gap = 3; // gap from the left page-nav arrow, and from the QR code to text
    const int32_t left_edge =
        2 + icon_get_width(&I_arrow_left) + side_gap + 2; // whole block nudged right

    const int32_t qr_w = icon_get_width(&I_qrcode);
    const int32_t qr_h = icon_get_height(&I_qrcode);
    const int32_t top = page_content_top(canvas);
    // Same gap above (to the subtitle) and below (to the screen edge).
    const int32_t qr_y = top + (64 - top - qr_h) / 2 + 1;
    canvas_draw_icon(canvas, left_edge, qr_y, &I_qrcode);

    const int32_t text_x = left_edge + qr_w + side_gap + 2; // whole text block nudged right

    canvas_set_font(canvas, FontSecondary);
    const int32_t line_h = canvas_get_font_params(canvas, FontSecondary)->height;
    const int32_t gap_1_2 = 2; // extra space between "Thank you," and "please support"
    const int32_t gap_2_3 = 5; // extra space between "please support" and "on GitHub"
    const int32_t text_h = line_h * 3 + gap_1_2 + gap_2_3;
    const int32_t top_lines_up = 1; // lines 1 and 2 nudged up
    const int32_t line3_down = 1; // line 3 nudged down
    const int32_t icon_up_from_line = 1; // icons stay put in world coords despite line3_down
    int32_t line_y =
        top + (64 - top - text_h) / 2; // keep the whole block centered alongside the QR code

    canvas_draw_str_aligned(
        canvas, text_x, line_y - top_lines_up, AlignLeft, AlignTop, "Thank you,");
    line_y += line_h + gap_1_2;
    canvas_draw_str_aligned(
        canvas, text_x, line_y - top_lines_up, AlignLeft, AlignTop, "please support");
    line_y += line_h + gap_2_3;
    const int32_t line3_y = line_y + line3_down;
    const int32_t letter_gap = 1; // extra space between letters, "on GitHub" only
    // No bold weight exists for bitmap fonts on this platform - faux-bold by drawing this last
    // line twice, offset by a pixel, to thicken the strokes; the star/heart icons trail after it.
    draw_tracked_str(canvas, text_x, line3_y, letter_gap, "on GitHub");
    draw_tracked_str(canvas, text_x + 1, line3_y, letter_gap, "on GitHub");
    const int32_t icons_x = text_x + tracked_str_width(canvas, letter_gap, "on GitHub") + 1 + 3;
    const int32_t icons_y_center = line3_y + line_h / 2 - icon_up_from_line;
    canvas_draw_icon(canvas, icons_x, icons_y_center - icon_get_height(&I_star) / 2, &I_star);
    canvas_draw_icon(
        canvas,
        icons_x + icon_get_width(&I_star) + 2,
        icons_y_center - icon_get_height(&I_heart) / 2,
        &I_heart);
}

void draw_info_screen(Canvas* canvas, uint8_t page) {
    // Page-nav arrows flush to the screen edges, vertically centered, hinting that left/right
    // cycle between pages.
    canvas_draw_icon(canvas, 2, 32 - icon_get_height(&I_arrow_left) / 2, &I_arrow_left);
    canvas_draw_icon(
        canvas,
        128 - 2 - icon_get_width(&I_arrow_right),
        32 - icon_get_height(&I_arrow_right) / 2,
        &I_arrow_right);

    if(page == 0) {
        draw_options_page(canvas);
    } else {
        draw_about_page(canvas);
    }
}
