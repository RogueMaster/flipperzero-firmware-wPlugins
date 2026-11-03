#include "clipboard_screen.h"
#include "companion_version.h"
#include "companion_qr.h"
#include "transfer_protocol.h"

#include <furi.h>
#include <gui/elements.h>
#include <string.h>

void clipboard_screen_info(ClipboardScreen* screen, bool qr) {
    memset(screen, 0, sizeof(*screen));
    screen->selected = -1;
    screen->companion_qr = qr;
    if(qr) return;
    snprintf(screen->left, sizeof(screen->left), "Back");
    snprintf(screen->center, sizeof(screen->center), "QR code");
    snprintf(screen->lines[0], sizeof(screen->lines[0]), "Version " CLIPBOARD_APP_VERSION);
    snprintf(screen->lines[1], sizeof(screen->lines[1]), "Protocol %u", CLIPBOARD_WIRE_VERSION);
    snprintf(
        screen->lines[2],
        sizeof(screen->lines[2]),
        "Build API %u.%u",
        CLIPBOARD_BUILD_API_MAJOR,
        CLIPBOARD_BUILD_API_MINOR);
    snprintf(screen->lines[3], sizeof(screen->lines[3]), "GPL-3.0-only");
}

static void draw_status_line(Canvas* canvas, const char* text, size_t length, int32_t baseline) {
    FuriString* line = furi_string_alloc();
    furi_string_set_strn(line, text, length);
    elements_string_fit_width(canvas, line, 120);
    canvas_draw_str_aligned(
        canvas, 64, baseline, AlignCenter, AlignBottom, furi_string_get_cstr(line));
    furi_string_free(line);
}

void clipboard_screen_draw(Canvas* canvas, const ClipboardScreen* screen) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    if(screen->companion_qr) {
        canvas_draw_str(canvas, 4, 10, "Companions");
        canvas_draw_str(canvas, 4, 23, "v" CLIPBOARD_APP_VERSION);
        canvas_draw_str(canvas, 4, 35, "Mac + Flipper");
        canvas_draw_str(canvas, 4, 47, "GitHub release");
        canvas_draw_xbm(
            canvas,
            127 - CLIPBOARD_QR_SIZE,
            2,
            CLIPBOARD_QR_SIZE,
            CLIPBOARD_QR_SIZE,
            clipboard_companion_qr);
        elements_button_left(canvas, "Back");
        return;
    }
    canvas_draw_str(canvas, 4, 10, "BLE Clipboard " CLIPBOARD_APP_VERSION);
    canvas_draw_line(canvas, 0, 14, 127, 14);

    for(unsigned i = 0; i < 4; i++) {
        if(screen->selected == (int8_t)i) {
            canvas_draw_box(canvas, 0, 15 + i * 9, 128, 9);
            canvas_set_color(canvas, ColorWhite);
        }
        draw_status_line(canvas, screen->lines[i], strlen(screen->lines[i]), 23 + i * 9);
        canvas_set_color(canvas, ColorBlack);
    }
    if(screen->left[0]) elements_button_left(canvas, screen->left);
    if(screen->center[0]) elements_button_center(canvas, screen->center);
    if(screen->right[0]) elements_button_right(canvas, screen->right);
}
