#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"
#include "../utils/draw.h"
#include "../utils/notification.h"
#include "../icon.h"
#include "canvas.h"
#include <iconedit_icons.h>

#define MAX_WIDTH  128
#define MAX_HEIGHT 64

static struct {
    int w;
    int h;
    uint8_t digits[6]; // 3 x 3
    uint8_t curr_digit;
    bool ok_enabled;
} newModel = {.w = 10, .h = 10, .digits = {0, 1, 0, 0, 1, 0}, .curr_digit = 1, .ok_enabled = true};

bool new_icon_check_digits() {
    newModel.w = newModel.digits[0] * 100 + newModel.digits[1] * 10 + newModel.digits[2];
    newModel.h = newModel.digits[3] * 100 + newModel.digits[4] * 10 + newModel.digits[5];
    if(newModel.w == 0 || newModel.h == 0) {
        return false;
    }
    if(newModel.w > MAX_WIDTH || newModel.h > MAX_HEIGHT) {
        return false;
    }
    return true;
}

void new_icon_draw(Canvas* canvas, void* context) {
    UNUSED(canvas);
    UNUSED(context);

    int x = 18;
    int y = 20;
    int rw = 90;
    int rh = 20;
    ie_draw_modal_panel_frame(canvas, x, y, rw, rh);

    int dw = 7;
    int xpad = 8;
    int ypad = 6;
    for(int i = 0; i < 6; i++) {
        char buf[4];

        snprintf(buf, sizeof(buf), "%d", newModel.digits[i]);
        canvas_draw_str_aligned(
            canvas, x + xpad + i * dw + 3, y + ypad, AlignCenter, AlignTop, buf);
        if(newModel.curr_digit == i) {
            // draw up/down arrow thingies too
            canvas_draw_icon(canvas, x + xpad + i * dw + 1, y + ypad - 5, &I_iet_smArrowU);
            canvas_draw_icon(canvas, x + xpad + i * dw + 1, y + ypad + 9, &I_iet_smArrowD);
        }
        if(i == 2) {
            // draw the X
            canvas_draw_str_aligned(
                canvas, x + 10 + xpad + i * dw + 3, y + ypad - 1, AlignCenter, AlignTop, "x");
            x += 12;
        }
    }
    canvas_draw_str_aligned(
        canvas, x + 10 + xpad + 5 * dw + 3, y + ypad, AlignCenter, AlignTop, "=");

    // are the dimensions good? Maybe make this icons of checkmark / X?
    canvas_draw_str_aligned(
        canvas,
        x + 10 + xpad + 7 * dw,
        y + ypad,
        AlignCenter,
        AlignTop,
        newModel.ok_enabled ? "Y" : "N");
}

bool new_icon_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    UNUSED(app);
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyLeft: {
            newModel.curr_digit -= newModel.curr_digit > 0;
            break;
        }
        case InputKeyRight: {
            newModel.curr_digit += newModel.curr_digit < 5;
            break;
        }
        case InputKeyUp:
            newModel.digits[newModel.curr_digit] = (newModel.digits[newModel.curr_digit] + 1) % 10;
            newModel.ok_enabled = new_icon_check_digits();
            break;
        case InputKeyDown:
            newModel.digits[newModel.curr_digit] = (newModel.digits[newModel.curr_digit] + 9) % 10;
            newModel.ok_enabled = new_icon_check_digits();
            break;
        case InputKeyOk:
            // set the new dimensions
            if(new_icon_check_digits()) {
                ie_icon_reset(app->icon, newModel.w, newModel.h, NULL);
                furi_string_set_str(app->icon->name, NEW_ICON_DEFAULT_NAME);
                canvas_initialize(app->icon, app->settings.canvas_scale);
                // user just created a new icon, place them directly into Tools!
                tabbar_set_selected_tab(TabTools);
                app->panel = Panel_Tools;
                app->dirty = true;
            } else {
                notify_error(app);
            }
            break;
        case InputKeyBack:
            // bail - discard new file dimensions
            app->panel = Panel_File;
            break;
        default:
            break;
        }
    }
    return consumed;
}
