#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"
#include "../utils/draw.h"
#include "../utils/notification.h"
#include <iconedit_icons.h>

static uint8_t fps = 4;

void fps_set_fps(uint8_t new_fps) {
    fps = new_fps;
}

void fps_draw(Canvas* canvas, void* context) {
    UNUSED(context);

    size_t cw = canvas_width(canvas);
    size_t ch = canvas_height(canvas);
    size_t pw = 50;
    size_t ph = 15;
    int x = (cw - pw) / 2;
    int y = (ch - ph) / 2;

    ie_draw_modal_panel_frame(canvas, x, y, pw, ph);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d fps", fps);
    canvas_draw_str_aligned(canvas, cw / 2, ch / 2 - 5, AlignCenter, AlignTop, buf);
    canvas_draw_icon(canvas, (cw / 2) - 16 - 5, (ch / 2) - 3, &I_iet_smArrowL);
    canvas_draw_icon(canvas, (cw / 2) + 16, (ch / 2) - 3, &I_iet_smArrowR);
}

bool fps_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyLeft: {
            if(fps - 1 < 1) {
                notify_error(app);
            } else {
                fps -= 1;
            }
            break;
        }
        case InputKeyRight: {
            if(fps + 1 > 15) {
                notify_error(app);
            } else {
                fps += 1;
            }
            break;
        }
        case InputKeyOk:
            app->icon->frame_rate = fps;
            app->panel = Panel_Tools;
            app->dirty = true;
            break;
        case InputKeyBack:
            app->panel = Panel_Tools;
            break;
        default:
            break;
        }
    }
    return consumed;
}
