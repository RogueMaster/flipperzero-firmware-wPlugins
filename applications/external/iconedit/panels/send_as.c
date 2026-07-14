#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"
#include "../utils/draw.h"

typedef enum {
    Send_START,
    Send_PNG = Send_START,
    Send_C,
    Send_BMX,
    Send_COUNT,
    Send_NONE
} SendMode;

typedef enum {
    Send_Frame,
    Send_Anim,
} SendType;

const char* SendModeStr[Send_COUNT] = {
    "PNG",
    ".C",
    "BMX",
};
const SendMode Send_UP[Send_COUNT] = {
    Send_NONE,
    Send_PNG,
    Send_C,
};
const SendMode Send_DOWN[Send_COUNT] = {Send_C, Send_BMX, Send_NONE};

static struct {
    SendMode send_mode;
    SendType send_type;
} sendModel = {.send_mode = Send_PNG};

bool send_as_anim_capable(SendMode mode) {
    return (mode == Send_PNG || mode == Send_C);
}

void send_as_draw(Canvas* canvas, void* context) {
    IconEdit* app = context;
    bool is_anim = app->icon->frame_count > 1;

    int pad = 2; // outside padding between frame and items
    int elem_h = is_anim ? 9 : 7;
    int elem_w = is_anim ? 76 : 22;
    int elem_pad = 2; // the space between

    // draw an empty panel frame
    int rw = elem_w + (pad * 2) + (elem_pad * 2);
    int rh = (elem_h + (elem_pad * 2)) * Send_COUNT + (pad * 2);
    int x = (canvas_width(canvas) - rw) / 2;
    int y = (canvas_height(canvas) - rh) / 2;
    ie_draw_modal_panel_frame(canvas, x, y, rw, rh);

    // draw Send menu items
    for(SendMode mode = Send_START; mode < Send_COUNT; mode++) {
        // draw the mode name (PNG, XBM, etc)
        canvas_draw_str_aligned(
            canvas,
            x + pad + elem_pad + 1,
            y + pad + elem_pad + mode * (elem_h + elem_pad * 2) + is_anim,
            AlignLeft,
            AlignTop,
            SendModeStr[mode]);

        if(mode == sendModel.send_mode) {
            // highlight the selected item - but differently if anim enabled
            if(send_as_anim_capable(mode) && is_anim) {
                canvas_draw_rframe(
                    canvas,
                    x + pad,
                    y + pad + (elem_h + (elem_pad * 2)) * mode,
                    elem_w + elem_pad * 2,
                    elem_h + elem_pad * 2,
                    1);
                canvas_draw_rbox(
                    canvas,
                    x + pad + elem_pad + 1 + 49 - 25,
                    y + pad + (elem_h + (elem_pad * 2)) * mode +
                        (sendModel.send_type == Send_Frame ? 0 : 6),
                    50,
                    elem_h / 2 + elem_pad * 2 - 1,
                    1);
                canvas_set_color(canvas, ColorXOR);
                int32_t curr_y = y + pad + elem_pad + mode * (elem_h + elem_pad * 2);
                char frame_str[16] = {};
                snprintf(
                    frame_str,
                    sizeof(frame_str),
                    "%s FRAME %d %s",
                    app->icon->current_frame == 0 ? " " : "<",
                    app->icon->current_frame + 1,
                    app->icon->current_frame == app->icon->frame_count - 1 ? " " : ">");
                ie_draw_str(
                    canvas,
                    x + pad + elem_pad + 1 + 49,
                    curr_y - 1,
                    AlignCenter,
                    AlignTop,
                    FontMicro,
                    frame_str);
                ie_draw_str(
                    canvas,
                    x + pad + elem_pad + 1 + 49,
                    curr_y + 5,
                    AlignCenter,
                    AlignTop,
                    FontMicro,
                    "ANIM");
                canvas_set_color(canvas, ColorBlack);
            } else {
                // hightlight the selected item
                canvas_set_color(canvas, ColorXOR);
                canvas_draw_rbox(
                    canvas,
                    x + pad,
                    y + pad + (elem_h + (elem_pad * 2)) * mode,
                    elem_w + elem_pad * 2,
                    elem_h + elem_pad * 2,
                    0);
                canvas_set_color(canvas, ColorBlack);
            }
        }
    }
}

bool send_as_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool is_anim = app->icon->frame_count > 1;

    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp: {
            if(send_as_anim_capable(sendModel.send_mode) && is_anim &&
               sendModel.send_type == Send_Anim) {
                sendModel.send_type = Send_Frame;
                break;
            }
            SendMode next_send = Send_UP[sendModel.send_mode];
            if(next_send != Send_NONE) {
                sendModel.send_mode = next_send;
                if(send_as_anim_capable(sendModel.send_mode) && is_anim) {
                    sendModel.send_type = Send_Anim;
                }
            }
            break;
        }
        case InputKeyDown: {
            if(send_as_anim_capable(sendModel.send_mode) && is_anim &&
               sendModel.send_type == Send_Frame) {
                sendModel.send_type = Send_Anim;
                break;
            }
            SendMode next_send = Send_DOWN[sendModel.send_mode];
            if(next_send != Send_NONE) {
                sendModel.send_mode = next_send;
                if(send_as_anim_capable(sendModel.send_mode) && is_anim) {
                    sendModel.send_type = Send_Frame;
                }
            }
            break;
        }
        case InputKeyLeft: {
            if(is_anim && send_as_anim_capable(sendModel.send_mode)) {
                app->icon->current_frame -= app->icon->current_frame > 0;
            }
            break;
        }
        case InputKeyRight: {
            if(is_anim && send_as_anim_capable(sendModel.send_mode)) {
                app->icon->current_frame += app->icon->current_frame < app->icon->frame_count - 1;
            }
            break;
        }
        case InputKeyBack: {
            consumed = false; // send control back to File panel
            app->panel = Panel_File;
            break;
        }
        case InputKeyOk: {
            switch(sendModel.send_mode) {
            case Send_PNG: {
                app->panel = Panel_SendUSB;
                if(!is_anim || sendModel.send_type == Send_Frame) {
                    send_usb_start(app->icon, SendAsPNG, true);
                } else {
                    send_usb_start(app->icon, SendAsPNG, false);
                }
                break;
            }
            case Send_C: {
                app->panel = Panel_SendUSB;
                if(!is_anim || sendModel.send_type == Send_Frame) {
                    send_usb_start(app->icon, SendAsC, true);
                } else {
                    send_usb_start(app->icon, SendAsC, false);
                }
                break;
            }
            case Send_BMX: {
                app->panel = Panel_SendUSB;
                send_usb_start(app->icon, SendAsBMX, true);
                break;
            }
            default:
                break;
            }
            consumed = false;
            break;
        }
        default:
            break;
        }
    }
    return consumed;
}
