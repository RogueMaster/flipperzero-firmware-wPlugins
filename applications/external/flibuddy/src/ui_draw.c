#include <gui/canvas.h>
#include <gui/elements.h>

#include <string.h>

#include "ui_draw.h"
#include "ui_icons.h"
#include "ui_text.h"

#define W           128
#define H           64
#define RULE_Y      10 /* the dotted rule under a header */
#define LINE_H      9 /* FontSecondary rows */
#define LIST_Y      12
#define LIST_ROW_H  12
#define TITLE_MAX_W 114 /* left of the transport icon */
#define ERROR_LINES 3
#define QR_X        67 /* Get app: 58x58, 3 px clear to the right, top and bottom */
#define QR_Y        3
#define GET_APP_W   62 /* the text left of the QR code */

/* The firmware's chips (gui/elements.c): 12 px on the bottom row, the
 * label 3 px above the bottom, three slanted corner columns on either side.
 * A side chip is the label + 13 px wide, the OK chip the label + 12 px. */
#define CHIP_H        12
#define CHIP_SIDE_PAD 13
#define CHIP_OK_PAD   12
#define CHIP_CORNER   3
#define CHIP_CLEAR    2 /* blank columns kept between an OK chip moved aside and its neighbour */

/* ---- text ---- */

static uint16_t measure(void* ctx, const char* text) {
    return canvas_string_width(ctx, text);
}

/* Text with its top-left corner at (x, y), cut to max_w. */
static void text_fit(Canvas* c, int32_t x, int32_t y, uint16_t max_w, const char* s) {
    char buf[UI_TEXT_MAX];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ui_text_fit(buf, max_w, measure, c);
    canvas_draw_str_aligned(c, x, y, AlignLeft, AlignTop, buf);
}

static void text_at(Canvas* c, int32_t x, int32_t y, const char* s) {
    text_fit(c, x, y, (uint16_t)(W - x), s);
}

static void text_center(Canvas* c, int32_t y, const char* s) {
    char buf[UI_TEXT_MAX];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ui_text_fit(buf, W, measure, c);
    canvas_draw_str_aligned(c, W / 2, y, AlignCenter, AlignTop, buf);
}

/* ---- widgets ---- */

static void bitmap(Canvas* c, int32_t x, int32_t y, const UiBitmap* b) {
    canvas_draw_xbm(c, x, y, b->width, b->height, b->bits);
}

static void icon(Canvas* c, const UiFrame* f) {
    if(f->icon == UiIconUsb) bitmap(c, 118, 3, &ui_icon_usb);
    if(f->icon == UiIconBt) bitmap(c, 121, 0, &ui_icon_bt);
}

static void dotted_rule(Canvas* c, int32_t y) {
    for(int32_t x = 0; x < W; x += 2)
        canvas_draw_dot(c, x, y);
}

static void header_at(Canvas* c, const UiFrame* f, int32_t x) {
    canvas_set_font(c, FontPrimary);
    text_fit(c, x, 1, (uint16_t)(TITLE_MAX_W - x), f->title);
    icon(c, f);
    dotted_rule(c, RULE_Y);
    canvas_set_font(c, FontSecondary);
}

static void header(Canvas* c, const UiFrame* f) {
    header_at(c, f, 2);
}

/* elements_button_center at x in place of the centre. */
static void chip_ok_at(Canvas* c, int32_t x, const char* s) {
    const int32_t y = H;
    const int32_t w = canvas_string_width(c, s) + CHIP_OK_PAD;
    canvas_draw_box(c, x, y - CHIP_H, w, CHIP_H);
    for(int32_t k = 0; k < CHIP_CORNER; k++) {
        canvas_draw_line(c, x - 1 - k, y, x - 1 - k, y - CHIP_H + k);
        canvas_draw_line(c, x + w + k, y, x + w + k, y - CHIP_H + k);
    }
    canvas_invert_color(c);
    bitmap(c, x + 1, y - ui_icon_ok_button.height - 3, &ui_icon_ok_button);
    canvas_draw_str(c, x + 1 + ui_icon_ok_button.width + 3, y - 3, s);
    canvas_invert_color(c);
}

/* The OK chip is centred unless that brings it closer than CHIP_CLEAR to
 * the ◀ or ▶ chip: then it moves aside (Waiting's Get app and Settings).
 * The ◀ limit is applied last, so if both side chips crowded the centre the
 * OK chip would keep clear of ◀ and touch ▶. No screen has such a trio:
 * with the firmware widths only Waiting (no ◀ chip) moves at all. */
static int32_t chip_ok_x(Canvas* c, const UiFrame* f) {
    int32_t w = canvas_string_width(c, f->center) + CHIP_OK_PAD;
    int32_t x = (W - w) / 2;
    if(f->right[0]) {
        int32_t right_starts =
            W - (canvas_string_width(c, f->right) + CHIP_SIDE_PAD) - CHIP_CORNER;
        int32_t max_x = right_starts - CHIP_CLEAR - 1 - CHIP_CORNER - w + 1;
        if(x > max_x) x = max_x;
    }
    if(f->left[0]) {
        int32_t left_ends = canvas_string_width(c, f->left) + CHIP_SIDE_PAD + CHIP_CORNER - 1;
        int32_t min_x = left_ends + CHIP_CLEAR + 1 + CHIP_CORNER;
        if(x < min_x) x = min_x;
    }
    return x;
}

static void chips(Canvas* c, const UiFrame* f) {
    canvas_set_font(c, FontSecondary);
    if(f->left[0]) elements_button_left(c, f->left);
    if(f->center[0]) {
        int32_t centred = (W - (canvas_string_width(c, f->center) + CHIP_OK_PAD)) / 2;
        int32_t x = chip_ok_x(c, f);
        if(x == centred) {
            elements_button_center(c, f->center);
        } else {
            chip_ok_at(c, x, f->center);
        }
    }
    if(f->right[0]) elements_button_right(c, f->right);
}

/* Inverted text in a rounded bar from x, as wide as the text. */
static void inverted(Canvas* c, int32_t x, int32_t y, const char* s) {
    char buf[UI_TEXT_MAX];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ui_text_fit(buf, (uint16_t)(W - x - 6), measure, c);
    uint16_t w = canvas_string_width(c, buf) + 6;
    canvas_draw_rbox(c, x, y, w, LINE_H, 2);
    canvas_set_color(c, ColorWhite);
    canvas_draw_str_aligned(c, x + 3, y + 1, AlignLeft, AlignTop, buf);
    canvas_set_color(c, ColorBlack);
}

static void list(Canvas* c, const UiFrame* f) {
    bool scroll = f->count > APP_LIST_ROWS;
    for(uint8_t k = 0; k < APP_LIST_ROWS; k++) {
        uint8_t i = f->top + k;
        if(i >= f->count) break;
        int32_t y = LIST_Y + k * LIST_ROW_H;
        bool on = i == f->sel;
        if(on) {
            canvas_draw_rbox(c, 0, y, scroll ? 123 : W, LIST_ROW_H - 1, 2);
            canvas_set_color(c, ColorWhite);
        }
        int32_t value_x = 117;
        int32_t item_x = 5;
        if(f->checks) {
            bitmap(c, 4, y + 3, &ui_icon_check);
            item_x = 13;
        }
        if(f->value[i][0]) {
            value_x -= canvas_string_width(c, f->value[i]);
            canvas_draw_str_aligned(c, value_x, y + 2, AlignLeft, AlignTop, f->value[i]);
            if(on && !f->fixed[i]) {
                bitmap(c, value_x - 6, y + 3, &ui_icon_arrow_left);
                bitmap(c, 119, y + 3, &ui_icon_arrow_right);
            }
        }
        text_fit(c, item_x, y + 2, (uint16_t)(value_x - item_x - 8), f->item[i]);
        canvas_set_color(c, ColorBlack);
    }
    if(scroll) elements_scrollbar_pos(c, W, LIST_Y, H - LIST_Y, f->sel, f->count);
}

static uint32_t isqrt(uint32_t n) {
    uint32_t r = 0;
    while((r + 1) * (r + 1) <= n)
        r++;
    return r;
}

/* A disc of radius r with the design's slightly fuller edge. */
static bool in_disc(int32_t dx, int32_t dy, int32_t r) {
    return dx * dx + dy * dy <= r * r + (r * 6) / 10;
}

static void clear_disc(Canvas* c, int32_t cx, int32_t cy, int32_t r) {
    canvas_set_color(c, ColorWhite);
    canvas_draw_disc(c, cx, cy, r);
    canvas_set_color(c, ColorBlack);
}

static void swatch(Canvas* c, int32_t cx, int32_t cy, int32_t r, uint8_t lum) {
    clear_disc(c, cx, cy, r);
    for(int32_t dy = -r; dy <= r; dy++)
        for(int32_t dx = -r; dx <= r; dx++)
            if(in_disc(dx, dy, r) && ui_bayer_ink(cx + dx, cy + dy, lum))
                canvas_draw_dot(c, cx + dx, cy + dy);
    canvas_draw_circle(c, cx, cy, r);
}

static void spool(Canvas* c, int32_t cx, int32_t cy, int32_t r) {
    clear_disc(c, cx, cy, r);
    canvas_draw_circle(c, cx, cy, r);
    for(int32_t rr = r - 3; rr * 100 > r * 45; rr -= 2)
        canvas_draw_circle(c, cx, cy, rr);
    int32_t hub = (r * 38 + 50) / 100;
    clear_disc(c, cx, cy, hub);
    canvas_draw_circle(c, cx, cy, hub);
    int32_t axle = (r * 14 + 50) / 100;
    canvas_draw_disc(c, cx, cy, axle < 1 ? 1 : axle);
}

/* An arc of radius r over ±0.5 rad to the right (dir 1) or left (-1). */
static void arc(Canvas* c, int32_t cx, int32_t cy, int32_t r, int32_t dir) {
    int32_t reach = r * 479 / 1000; /* r sin 0.5 */
    for(int32_t dy = -reach; dy <= reach; dy++)
        canvas_draw_dot(c, cx + dir * (int32_t)isqrt((uint32_t)(r * r - dy * dy)), cy + dy);
}

static void waves(Canvas* c, int32_t cx, int32_t cy, int32_t r0, uint32_t t) {
    for(uint32_t i = 0; i < 3; i++)
        if(t % 4 > i) arc(c, cx, cy, r0 + (int32_t)i * 5, 1);
}

static void bouncing_dots(Canvas* c, int32_t x, int32_t y, uint8_t anim) {
    for(int32_t k = 0; k < 3; k++) {
        int32_t up = anim % 3 == k ? 2 : 0;
        canvas_draw_box(c, x + k * 8, y - up, 4, 4);
    }
}

/* ---- screens ---- */

static void draw_waiting(Canvas* c, const UiFrame* f) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 2, 1, AlignLeft, AlignTop, f->title);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 126, 2, AlignRight, AlignTop, f->note);
    dotted_rule(c, RULE_Y);
    /* USB, then Bluetooth, as in the design */
    if(f->line_icon[0] == UiIconUsb) bitmap(c, 4, 17, &ui_icon_usb);
    text_at(c, 18, 15, f->line[0]);
    if(f->line_icon[1] == UiIconBt) bitmap(c, 6, 25, &ui_icon_bt);
    text_at(c, 18, 26, f->line[1]);
    text_center(c, 39, f->line[2]);
    chips(c, f);
}

static void draw_ready(Canvas* c, const UiFrame* f) {
    header(c, f);
    if(f->bar[0]) {
        canvas_draw_box(c, 0, 12, W, 10);
        canvas_set_color(c, ColorWhite);
        bitmap(c, 3, 14, &ui_icon_check);
        text_fit(c, 13, 13, W - 15, f->bar);
        canvas_set_color(c, ColorBlack);
        spool(c, 12, 37, 11);
        waves(c, 12, 37, 14, f->anim);
        canvas_set_font(c, FontPrimary);
        text_at(c, 40, 25, f->line[0]);
        canvas_set_font(c, FontSecondary);
    } else {
        spool(c, 14, 31, 13);
        waves(c, 14, 31, 16, f->anim);
        canvas_set_font(c, FontPrimary);
        text_at(c, 42, 16, f->line[0]);
        canvas_set_font(c, FontSecondary);
    }
    int32_t y = f->bar[0] ? 35 : 29;
    text_at(c, f->bar[0] ? 40 : 42, y, f->line[1]);
    text_at(c, f->bar[0] ? 40 : 42, y + LINE_H, f->line[2]);
    chips(c, f);
}

static void draw_reading(Canvas* c, const UiFrame* f) {
    header(c, f);
    spool(c, 64, 30, 11);
    waves(c, 64, 30, 14, f->anim + 1u);
    arc(c, 64, 30, 14 + (f->anim % 3) * 4, -1);
    text_center(c, 46, f->line[0]);
}

static void draw_looking_up(Canvas* c, const UiFrame* f) {
    header(c, f);
    canvas_set_font(c, FontPrimary);
    text_center(c, 16, f->line[0]);
    canvas_set_font(c, FontSecondary);
    bouncing_dots(c, 54, 32, f->anim);
    text_center(c, 42, f->line[1]);
}

static void draw_result(Canvas* c, const UiFrame* f) {
    if(f->mark == UiMarkSwatch) {
        swatch(c, 6, 5, 4, f->lum);
    } else {
        canvas_draw_circle(c, 6, 5, 4);
    }
    header_at(c, f, 14);
    text_at(c, 2, 13, f->line[0]);
    if(f->in_base) {
        bitmap(c, 2, 14 + LINE_H, &ui_icon_check);
        text_at(c, 11, 13 + LINE_H, f->line[1]);
    } else {
        text_at(c, 2, 13 + LINE_H, f->line[1]);
    }
    if(f->bar[0]) {
        inverted(c, 0, 31, f->bar);
    } else if(f->level >= 0) {
        /* Remaining against the label: filled part left, frame for the rest. */
        canvas_draw_rframe(c, 0, 32, 58, 8, 2);
        size_t fill = (size_t)54 * (size_t)(f->level > 1000 ? 1000 : f->level) / 1000;
        if(fill > 0) canvas_draw_box(c, 2, 34, fill, 4);
        text_at(c, 62, 31, f->line[2]);
    } else {
        text_at(c, 2, 13 + 2 * LINE_H, f->line[2]);
    }
    chips(c, f);
}

static void draw_details(Canvas* c, const UiFrame* f) {
    header(c, f);
    for(uint8_t i = 0; i < f->count && i < UI_LINES; i++) {
        int32_t y = 13 + i * LINE_H;
        canvas_draw_str_aligned(c, 2, y, AlignLeft, AlignTop, f->key[i]);
        text_at(c, 30, y, f->line[i]);
    }
    if(f->more_up) bitmap(c, W - 6, 13, &ui_icon_arrow_up);
    if(f->more_down) bitmap(c, W - 6, H - 4, &ui_icon_arrow_down);
}

static void draw_confirm_delete(Canvas* c, const UiFrame* f) {
    bitmap(c, 4, 2, &ui_icon_warn);
    canvas_set_font(c, FontPrimary);
    text_fit(c, 17, 1, W - 19, f->title);
    canvas_set_font(c, FontSecondary);
    dotted_rule(c, 12);
    for(int i = 0; i < 3; i++)
        text_at(c, 2, 16 + i * LINE_H, f->line[i]);
    chips(c, f);
}

/* Net | Gross at the top right; the arrows show the cursor on it. */
static void mode_chip(Canvas* c, const UiFrame* f) {
    static const char* const labels[2] = {"Net", "Gross"};
    int32_t widths[2];
    int32_t x = W;
    for(int i = 1; i >= 0; i--) {
        widths[i] = canvas_string_width(c, labels[i]) + 6;
        x -= widths[i] + (i == 1 ? 0 : 1);
    }
    if(f->cursor == 0) {
        bitmap(c, x - 7, 0, &ui_icon_arrow_up);
        bitmap(c, x - 7, 6, &ui_icon_arrow_down);
    }
    for(int i = 0; i < 2; i++) {
        bool on = (i == 0) == f->net;
        if(on) {
            canvas_draw_rbox(c, x, 0, widths[i], 10, 2);
            canvas_set_color(c, ColorWhite);
        } else {
            canvas_draw_rframe(c, x, 0, widths[i], 10, 2);
        }
        canvas_draw_str_aligned(c, x + 3, 1, AlignLeft, AlignTop, labels[i]);
        canvas_set_color(c, ColorBlack);
        x += widths[i] + 1;
    }
}

static void draw_weight(Canvas* c, const UiFrame* f) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 2, 1, AlignLeft, AlignTop, f->title);
    canvas_set_font(c, FontSecondary);
    mode_chip(c, f);

    canvas_set_font(c, FontBigNumbers);
    int32_t gw = canvas_glyph_width(c, '0');
    int32_t gh = canvas_current_font_height(c);
    int32_t step = gw + 2;
    int32_t x0 = (W - (APP_WEIGHT_DIGITS * step + 10)) / 2;
    int32_t y0 = 14;
    /* leading zeros are dimmed, except the last digit */
    int lead = 0;
    while(lead < APP_WEIGHT_DIGITS - 1 && f->digit[lead] == 0)
        lead++;
    for(int k = 0; k < APP_WEIGHT_DIGITS; k++) {
        int32_t x = x0 + k * step;
        canvas_draw_glyph(c, x, y0 + gh, (uint16_t)('0' + f->digit[k]));
        if(k < lead) {
            canvas_set_color(c, ColorWhite);
            for(int32_t yy = y0; yy < y0 + gh + 1; yy++)
                for(int32_t xx = x; xx < x + gw; xx++)
                    if((xx + yy) % 2) canvas_draw_dot(c, xx, yy);
            canvas_set_color(c, ColorBlack);
        }
    }
    if(f->cursor > 0) canvas_draw_box(c, x0 + (f->cursor - 1) * step, y0 + gh + 3, gw, 2);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, x0 + APP_WEIGHT_DIGITS * step + 2, y0 + gh, "g");

    canvas_set_font(c, FontSecondary);
    if(f->bar[0]) {
        uint16_t w = canvas_string_width(c, f->bar) + 6;
        inverted(c, (W - w) / 2, 37, f->bar);
    } else {
        text_center(c, 38, f->line[0]);
    }
    bitmap(c, 1, 56, &ui_icon_arrow_left);
    bitmap(c, 5, 56, &ui_icon_arrow_right);
    canvas_draw_str_aligned(c, 10, 55, AlignLeft, AlignTop, "Move");
    chips(c, f);
    bitmap(c, 101, 55, &ui_icon_arrow_up);
    bitmap(c, 101, 59, &ui_icon_arrow_down);
    canvas_draw_str_aligned(c, 107, 55, AlignLeft, AlignTop, "Set");
}

static void draw_busy(Canvas* c, const UiFrame* f) {
    header(c, f);
    text_center(c, 17, f->line[0]);
    canvas_draw_rframe(c, 14, 29, 100, 8, 2);
    canvas_draw_box(c, 16 + (f->anim % 8) * 11, 31, 18, 4);
    text_center(c, 43, f->line[1]);
}

static void draw_saved(Canvas* c, const UiFrame* f) {
    for(int32_t k = 0; k < 3; k++) {
        canvas_draw_line(c, 50, 14 + k, 58, 22 + k);
        canvas_draw_line(c, 58, 22 + k, 76, 4 + k);
    }
    canvas_set_font(c, FontPrimary);
    text_center(c, 31, f->title);
    canvas_set_font(c, FontSecondary);
    text_center(c, 45, f->line[0]);
}

static void draw_toast(Canvas* c, const UiFrame* f) {
    bitmap(c, 3, 2, &ui_icon_check);
    canvas_set_font(c, FontPrimary);
    text_fit(c, 13, 1, TITLE_MAX_W - 13, f->title);
    canvas_set_font(c, FontSecondary);
    icon(c, f);
    dotted_rule(c, RULE_Y);
    text_at(c, 2, 13, f->line[0]);
    text_at(c, 2, 13 + LINE_H, f->line[1]);
    if(f->line[2][0]) {
        canvas_draw_box(c, 0, 33, (size_t)W * f->progress / UI_TOAST_MAX, 2);
        text_at(c, 2, 38, f->line[2]);
    }
    chips(c, f);
}

static void draw_link(Canvas* c, const UiFrame* f) {
    header(c, f);
    text_at(c, 2, 13, f->line[0]);
    text_at(c, 2, 13 + LINE_H, f->line[1]);
    bouncing_dots(c, 54, 36, f->anim);
    chips(c, f);
}

static void draw_error(Canvas* c, const UiFrame* f) {
    header(c, f);
    bitmap(c, 3, 15, &ui_icon_warn);
    char line[UI_TEXT_MAX];
    size_t pos = 0;
    for(int i = 0; i < ERROR_LINES; i++) {
        if(!ui_text_wrap(f->body, &pos, line, sizeof(line), W - 19, measure, c)) break;
        canvas_draw_str_aligned(c, 17, 14 + i * LINE_H, AlignLeft, AlignTop, line);
    }
    if(f->progress > 0) canvas_draw_box(c, 0, 50, (size_t)W * f->progress / UI_TOAST_MAX, 2);
    chips(c, f);
}

static void draw_get_app(Canvas* c, const UiFrame* f) {
    canvas_set_font(c, FontPrimary);
    text_fit(c, 2, 1, GET_APP_W, f->line[0]);
    canvas_set_font(c, FontSecondary);
    for(int i = 1; i < 4; i++)
        text_fit(c, 2, 13 + (i - 1) * LINE_H, GET_APP_W, f->line[i]);
    canvas_set_font(c, FontPrimary);
    text_fit(c, 2, 40, GET_APP_W, f->line[4]);
    bitmap(c, QR_X, QR_Y, &ui_icon_qr_get_app);
    chips(c, f);
}

static void draw_about(Canvas* c, const UiFrame* f) {
    bitmap(c, 6, 5, &ui_icon_app_2x);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 32, 6, AlignLeft, AlignTop, f->title);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 32, 17, AlignLeft, AlignTop, f->line[0]);
    dotted_rule(c, 29);
    for(int i = 1; i < 4; i++)
        text_at(c, 2, 32 + (i - 1) * LINE_H, f->line[i]);
}

void ui_draw(Canvas* c, const UiFrame* f) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);
    switch(f->screen) {
    case AppScreenWaiting:
        draw_waiting(c, f);
        break;
    case AppScreenReady:
        draw_ready(c, f);
        break;
    case AppScreenReading:
        draw_reading(c, f);
        break;
    case AppScreenLookingUp:
        draw_looking_up(c, f);
        break;
    case AppScreenResult:
        draw_result(c, f);
        break;
    case AppScreenMenu:
    case AppScreenSettings:
        header(c, f);
        list(c, f);
        break;
    case AppScreenDetails:
        draw_details(c, f);
        break;
    case AppScreenConfirmDelete:
        draw_confirm_delete(c, f);
        break;
    case AppScreenWeight:
        draw_weight(c, f);
        break;
    case AppScreenBusy:
        draw_busy(c, f);
        break;
    case AppScreenSaved:
        draw_saved(c, f);
        break;
    case AppScreenToast:
        draw_toast(c, f);
        break;
    case AppScreenLinkOnHost:
        draw_link(c, f);
        break;
    case AppScreenError:
        draw_error(c, f);
        break;
    case AppScreenAbout:
        draw_about(c, f);
        break;
    case AppScreenGetApp:
        draw_get_app(c, f);
        break;
    case AppScreenRecent:
        header(c, f);
        if(f->count == 0) {
            text_center(c, 30, f->line[0]);
        } else {
            list(c, f);
        }
        chips(c, f);
        break;
    }
}
