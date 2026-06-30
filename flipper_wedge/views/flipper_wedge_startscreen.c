#include "../flipper_wedge.h"
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/elements.h>

#define MODE_COUNT 5

static const char* mode_names[] = {
    "NFC",
    "RFID",
    "NDEF",
    "NFC -> RFID",
    "RFID -> NFC",
};

struct FlipperWedgeStartscreen {
    View* view;
    FlipperWedgeStartscreenCallback callback;
    void* context;
};

typedef struct {
    bool usb_connected;
    bool bt_connected;
    uint8_t mode;
    FlipperWedgeDisplayState display_state;
    char status_text[32];
    char uid_text[64];
} FlipperWedgeStartscreenModel;

void flipper_wedge_startscreen_set_callback(
    FlipperWedgeStartscreen* instance,
    FlipperWedgeStartscreenCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

void flipper_wedge_startscreen_draw(Canvas* canvas, FlipperWedgeStartscreenModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Flipper Wedge");

    // HID connection status
    canvas_set_font(canvas, FontSecondary);
    char status_line[32];
    if(model->usb_connected && model->bt_connected) {
        snprintf(status_line, sizeof(status_line), "USB: OK  BT: OK");
    } else if(model->usb_connected) {
        snprintf(status_line, sizeof(status_line), "USB: OK  BT: --");
    } else if(model->bt_connected) {
        snprintf(status_line, sizeof(status_line), "USB: --  BT: OK");
    } else {
        snprintf(status_line, sizeof(status_line), "No HID connection");
    }
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, status_line);

    bool connected = model->usb_connected || model->bt_connected;

    // Mode display with arrows
    canvas_set_font(canvas, FontPrimary);

    if(model->display_state == FlipperWedgeDisplayStateIdle) {
        // Show mode selector
        const char* mode_name = mode_names[model->mode];
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, mode_name);

        // Draw navigation arrows
        canvas_draw_str_aligned(canvas, 8, 28, AlignCenter, AlignTop, "<");
        canvas_draw_str_aligned(canvas, 120, 28, AlignCenter, AlignTop, ">");

        // Status and bottom buttons
        canvas_set_font(canvas, FontSecondary);
        if(connected) {
            canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, "Scanning...");
        } else {
            canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, "Connect USB or BT");
        }

        // Show Settings hint
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, "[OK] Settings");
    } else if(model->display_state == FlipperWedgeDisplayStateScanning) {
        // Scanning state
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Scanning...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, model->status_text);
    } else if(model->display_state == FlipperWedgeDisplayStateWaiting) {
        // Waiting for second tag
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Waiting...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, model->status_text);
    } else if(model->display_state == FlipperWedgeDisplayStateResult) {
        // Show scanned UID
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, model->uid_text);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, model->status_text);
    } else if(model->display_state == FlipperWedgeDisplayStateSent) {
        // Show "Sent"
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Sent");
    }
}

static void flipper_wedge_startscreen_model_init(FlipperWedgeStartscreenModel* const model) {
    model->usb_connected = false;
    model->bt_connected = false;
    model->mode = FlipperWedgeModeNfc;
    model->display_state = FlipperWedgeDisplayStateIdle;
    model->status_text[0] = '\0';
    model->uid_text[0] = '\0';
}

bool flipper_wedge_startscreen_input(InputEvent* event, void* context) {
    furi_assert(context);
    FlipperWedgeStartscreen* instance = context;

    if(event->type == InputTypeRelease || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyBack:
            if(event->type == InputTypeRelease) {
                instance->callback(FlipperWedgeCustomEventStartscreenBack, instance->context);
            }
            break;
        case InputKeyLeft:
            with_view_model(
                instance->view,
                FlipperWedgeStartscreenModel * model,
                {
                    if(model->display_state == FlipperWedgeDisplayStateIdle) {
                        if(model->mode == 0) {
                            model->mode = MODE_COUNT - 1;
                        } else {
                            model->mode--;
                        }
                        instance->callback(FlipperWedgeCustomEventModeChange, instance->context);
                    }
                },
                true);
            break;
        case InputKeyRight:
            with_view_model(
                instance->view,
                FlipperWedgeStartscreenModel * model,
                {
                    if(model->display_state == FlipperWedgeDisplayStateIdle) {
                        model->mode = (model->mode + 1) % MODE_COUNT;
                        instance->callback(FlipperWedgeCustomEventModeChange, instance->context);
                    }
                },
                true);
            break;
        case InputKeyOk:
            if(event->type == InputTypeRelease) {
                // Open Settings
                instance->callback(FlipperWedgeCustomEventOpenSettings, instance->context);
            }
            break;
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyMAX:
            break;
        }
    }
    return true;
}

void flipper_wedge_startscreen_exit(void* context) {
    furi_assert(context);
}

void flipper_wedge_startscreen_enter(void* context) {
    furi_assert(context);
    FlipperWedgeStartscreen* instance = (FlipperWedgeStartscreen*)context;
    UNUSED(instance);
    // Model is initialized during allocation and updated via setters
    // Don't reset state when view becomes active
}

FlipperWedgeStartscreen* flipper_wedge_startscreen_alloc() {
    FlipperWedgeStartscreen* instance = malloc(sizeof(FlipperWedgeStartscreen));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(FlipperWedgeStartscreenModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)flipper_wedge_startscreen_draw);
    view_set_input_callback(instance->view, flipper_wedge_startscreen_input);
    view_set_enter_callback(instance->view, flipper_wedge_startscreen_enter);
    view_set_exit_callback(instance->view, flipper_wedge_startscreen_exit);

    with_view_model(
        instance->view,
        FlipperWedgeStartscreenModel * model,
        { flipper_wedge_startscreen_model_init(model); },
        true);

    return instance;
}

void flipper_wedge_startscreen_free(FlipperWedgeStartscreen* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view, FlipperWedgeStartscreenModel * model, { UNUSED(model); }, true);
    view_free(instance->view);
    free(instance);
}

View* flipper_wedge_startscreen_get_view(FlipperWedgeStartscreen* instance) {
    furi_assert(instance);
    return instance->view;
}

void flipper_wedge_startscreen_set_connected_status(
    FlipperWedgeStartscreen* instance,
    bool usb_connected,
    bool bt_connected) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        FlipperWedgeStartscreenModel * model,
        {
            model->usb_connected = usb_connected;
            model->bt_connected = bt_connected;
        },
        true);
}

void flipper_wedge_startscreen_set_mode(
    FlipperWedgeStartscreen* instance,
    uint8_t mode) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        FlipperWedgeStartscreenModel * model,
        {
            model->mode = mode;
        },
        true);
}

uint8_t flipper_wedge_startscreen_get_mode(FlipperWedgeStartscreen* instance) {
    furi_assert(instance);
    uint8_t mode = 0;
    with_view_model(
        instance->view,
        FlipperWedgeStartscreenModel * model,
        {
            mode = model->mode;
        },
        false);
    return mode;
}

void flipper_wedge_startscreen_set_display_state(
    FlipperWedgeStartscreen* instance,
    FlipperWedgeDisplayState state) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        FlipperWedgeStartscreenModel * model,
        {
            model->display_state = state;
        },
        true);
}

void flipper_wedge_startscreen_set_status_text(
    FlipperWedgeStartscreen* instance,
    const char* text) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        FlipperWedgeStartscreenModel * model,
        {
            if(text) {
                snprintf(model->status_text, sizeof(model->status_text), "%s", text);
            } else {
                model->status_text[0] = '\0';
            }
        },
        true);
}

void flipper_wedge_startscreen_set_uid_text(
    FlipperWedgeStartscreen* instance,
    const char* text) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        FlipperWedgeStartscreenModel * model,
        {
            if(text) {
                snprintf(model->uid_text, sizeof(model->uid_text), "%s", text);
            } else {
                model->uid_text[0] = '\0';
            }
        },
        true);
}
