#include <gui/canvas.h>
#include <input/input.h>
#include <toolbox/compress.h>

#include "panels.h"
#include "../utils/draw.h"
#include "../utils/file_utils.h"

#include "../iconedit.h"
#include <iconedit_icons.h>
#include <gui/icon_i.h>

void about_draw(Canvas* canvas, void* context) {
    UNUSED(context);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, 22, AlignCenter, AlignBottom, APP_NAME);
    char version[10];
    snprintf(version, sizeof(version), "v%s", FAP_VERSION);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 32, 31, AlignCenter, AlignBottom, version);

    ie_draw_str(canvas, 2, 40, AlignLeft, AlignTop, Font5x7, "github.com");
    ie_draw_str(canvas, 8, 48, AlignLeft, AlignTop, Font5x7, "/rdefeo");
    ie_draw_str(canvas, 14, 56, AlignLeft, AlignTop, Font5x7, "/iconedit");

    // hijack the canvas and draw our app icon!
    // we should probably do all of this decoding once, and not on every draw :/

    const uint8_t* xbmc = icon_get_frame_data(&I_iconedit, 0);

    // #define ICON_DECOMPRESSOR_BUFFER_SIZE (128u * 64 / 8)
    CompressIcon* compress = compress_icon_alloc((128u * 64 / 8));
    uint8_t* xbm = NULL;
    compress_icon_decode(compress, xbmc, &xbm);

    uint8_t* pixel_data = NULL;
    pixel_data = xbm_decode(xbm, 10, 10);

    compress_icon_free(compress);

    // this next part is mostly copied direct from our canvas draw method,
    // but since we know we have a 10x10 icon, we can take some shortcuts
    // in computing where and how to draw
    int x_offset = 64 + 2; // top-left corner
    int y_offset = 0 + 2; // of our canvas area
    int scale = 6;

    // clear the canvas area
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 64, 0, 64, 64);
    canvas_set_color(canvas, ColorBlack);

    // draw
    for(size_t y = 0; y < 10; ++y) {
        for(size_t x = 0; x < 10; ++x) {
            if(pixel_data[y * 10 + x]) {
                canvas_draw_box(
                    canvas, x_offset + (x * scale), y_offset + (y * scale), scale, scale);
            }
        }
    }
    free(pixel_data);
}

bool about_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
        case InputKeyBack:
            app->panel = Panel_TabBar;
            break;
        default:
            break;
        }
    }
    return consumed;
}
