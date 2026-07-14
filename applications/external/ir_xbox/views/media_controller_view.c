#include "xbox_controller.h"
#include "media_controller_view.h"

#include <infrared_worker.h>
#include <infrared_transmit.h>

struct MediaControllerView {
    View* view;
    NotificationApp* notifications;
};

typedef struct {
    bool left_pressed;
    bool up_pressed;
    bool right_pressed;
    bool down_pressed;
    bool ok_pressed;
    bool back_pressed;
    bool connected;
} MediaControllerViewModel;

static void
    media_controller_view_draw_icon(Canvas* canvas, uint8_t x, uint8_t y, CanvasDirection dir) {
    if(dir == CanvasDirectionBottomToTop) {
        canvas_draw_icon(canvas, x - 7, y - 5, &I_Volup_Icon_11x11);
    } else if(dir == CanvasDirectionTopToBottom) {
        canvas_draw_icon(canvas, x - 7, y - 5, &I_Voldown_Icon_11x11);
    } else if(dir == CanvasDirectionRightToLeft) {
        canvas_draw_triangle(canvas, x, y, 8, 5, CanvasDirectionRightToLeft);
        canvas_draw_line(canvas, x - 5, y - 4, x - 5, y + 4);
    } else if(dir == CanvasDirectionLeftToRight) {
        canvas_draw_triangle(canvas, x - 4, y, 8, 5, CanvasDirectionLeftToRight);
        canvas_draw_line(canvas, x + 1, y - 4, x + 1, y + 4);
    }
}

static void media_controller_view_draw_arrow_button(
    Canvas* canvas,
    bool pressed,
    uint8_t x,
    uint8_t y,
    CanvasDirection direction) {
    canvas_draw_icon(canvas, x, y, &I_Button_18x18);
    if(pressed) {
        elements_slightly_rounded_box(canvas, x + 3, y + 2, 13, 13);
        canvas_set_color(canvas, ColorWhite);
    }
    media_controller_view_draw_icon(canvas, x + 11, y + 8, direction);
    canvas_set_color(canvas, ColorBlack);
}

static void media_controller_draw_wide_button(
    Canvas* canvas,
    bool pressed,
    uint8_t x,
    uint8_t y,
    char* text,
    const Icon* icon) {
    // canvas_draw_icon(canvas, 0, 25, &I_Space_65x18);
    elements_slightly_rounded_frame(canvas, x, y, 64, 17);
    if(pressed) {
        elements_slightly_rounded_box(canvas, x + 2, y + 2, 60, 13);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, x + 11, y + 4, icon);
    elements_multiline_text_aligned(canvas, x + 28, y + 12, AlignLeft, AlignBottom, text);
    canvas_set_color(canvas, ColorBlack);
}

static void media_controller_view_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    MediaControllerViewModel* model = context;

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 0, 0, AlignLeft, AlignTop, "Media");

    canvas_set_font(canvas, FontSecondary);

    canvas_draw_icon(canvas, 0, 12, &I_Pin_back_arrow_10x8);
    canvas_draw_str(canvas, 12, 20, "Hold");

    media_controller_view_draw_arrow_button(
        canvas, model->up_pressed, 23, 74, CanvasDirectionBottomToTop);
    media_controller_view_draw_arrow_button(
        canvas, model->down_pressed, 23, 110, CanvasDirectionTopToBottom);
    media_controller_view_draw_arrow_button(
        canvas, model->left_pressed, 0, 92, CanvasDirectionRightToLeft);
    media_controller_view_draw_arrow_button(
        canvas, model->right_pressed, 46, 92, CanvasDirectionLeftToRight);

    int buttons_post = 30;
    // Ok
    media_controller_draw_wide_button(
        canvas, model->ok_pressed, 0, buttons_post, "Play", &I_Ok_btn_9x9);
    // Back
    media_controller_draw_wide_button(
        canvas, model->back_pressed, 0, buttons_post + 19, "Mute", &I_Pin_back_arrow_10x8);
}

static void
    media_controller_view_process(MediaControllerView* media_controller_view, InputEvent* event) {
    with_view_model(
        media_controller_view->view,
        MediaControllerViewModel * model,
        {
            if(event->type == InputTypePress || event->type == InputTypeRepeat) {
                bool repeat = event->type == InputTypeRepeat;
                if(event->key == InputKeyUp) {
                    model->up_pressed = true;
                    send_xbox_ir(0xEF10, media_controller_view->notifications, repeat);
                } else if(event->key == InputKeyDown) {
                    model->down_pressed = true;
                    send_xbox_ir(0xEE11, media_controller_view->notifications, repeat);
                } else if(event->key == InputKeyLeft) {
                    model->left_pressed = true;
                    send_xbox_ir(0xE41B, media_controller_view->notifications, repeat);
                } else if(event->key == InputKeyRight) {
                    model->right_pressed = true;
                    send_xbox_ir(0xE51A, media_controller_view->notifications, repeat);
                } else if(event->key == InputKeyOk) {
                    model->ok_pressed = true;
                    send_xbox_ir(0x8F70, media_controller_view->notifications, repeat);
                } else if(event->key == InputKeyBack) {
                    model->back_pressed = true;
                    send_xbox_ir(0xF10E, media_controller_view->notifications, repeat);
                }
            } else if(event->type == InputTypeRelease) {
                if(event->key == InputKeyUp) {
                    model->up_pressed = false;
                } else if(event->key == InputKeyDown) {
                    model->down_pressed = false;
                } else if(event->key == InputKeyLeft) {
                    model->left_pressed = false;
                } else if(event->key == InputKeyRight) {
                    model->right_pressed = false;
                } else if(event->key == InputKeyOk) {
                    model->ok_pressed = false;
                } else if(event->key == InputKeyBack) {
                    model->back_pressed = false;
                }
            }
        },
        true);
}

static bool media_controller_view_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    MediaControllerView* media_controller_view = context;
    bool consumed = false;

    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        // LONG KEY BACK PRESS HANDLER
    } else {
        media_controller_view_process(media_controller_view, event);
        consumed = true;
    }

    return consumed;
}

MediaControllerView* media_controller_view_alloc(NotificationApp* notifications) {
    MediaControllerView* media_controller_view = malloc(sizeof(MediaControllerView));
    media_controller_view->view = view_alloc();
    media_controller_view->notifications = notifications;
    view_set_orientation(media_controller_view->view, ViewOrientationVertical);
    view_set_context(media_controller_view->view, media_controller_view);
    view_allocate_model(
        media_controller_view->view, ViewModelTypeLocking, sizeof(MediaControllerViewModel));
    view_set_draw_callback(media_controller_view->view, media_controller_view_draw_callback);
    view_set_input_callback(media_controller_view->view, media_controller_view_input_callback);

    return media_controller_view;
}

void media_controller_view_free(MediaControllerView* media_controller_view) {
    furi_assert(media_controller_view);
    view_free(media_controller_view->view);
    free(media_controller_view);
}

View* media_controller_view_get_view(MediaControllerView* media_controller_view) {
    furi_assert(media_controller_view);
    return media_controller_view->view;
}

void media_controller_view_set_connected_status(
    MediaControllerView* media_controller_view,
    bool connected) {
    furi_assert(media_controller_view);
    with_view_model(
        media_controller_view->view,
        MediaControllerViewModel * model,
        { model->connected = connected; },
        true);
}
