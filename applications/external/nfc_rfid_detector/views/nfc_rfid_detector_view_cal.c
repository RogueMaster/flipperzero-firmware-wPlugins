#include "nfc_rfid_detector_view_cal.h"

#include <gui/elements.h>
#include <nfc_rfid_detector_icons.h>
#include <input/input.h>
#include <stdio.h>

struct NfcRfidDetectorCal {
    View* view;
    NfcRfidDetectorCalCallback callback;
    void* context;
};

typedef struct {
    int16_t cal; // 0.1 ppm
} NfcRfidDetectorCalModel;

// filled arrow, with the point `distance` px from (center_x, center_y) in `direction`
static void nfc_rfid_detector_view_cal_arrow(
    Canvas* canvas,
    int32_t center_x,
    int32_t center_y,
    int32_t distance,
    CanvasDirection direction) {
    for(int32_t i = 0; i < 4; i++) {
        int32_t offset = distance - 3 + i; // base (7 wide) to point (1 wide)
        int32_t half_width = 3 - i;
        switch(direction) {
        case CanvasDirectionBottomToTop:
            canvas_draw_line(
                canvas,
                center_x - half_width,
                center_y - offset,
                center_x + half_width,
                center_y - offset);
            break;
        case CanvasDirectionTopToBottom:
            canvas_draw_line(
                canvas,
                center_x - half_width,
                center_y + offset,
                center_x + half_width,
                center_y + offset);
            break;
        case CanvasDirectionRightToLeft:
            canvas_draw_line(
                canvas,
                center_x - offset,
                center_y - half_width,
                center_x - offset,
                center_y + half_width);
            break;
        default:
            canvas_draw_line(
                canvas,
                center_x + offset,
                center_y - half_width,
                center_x + offset,
                center_y + half_width);
            break;
        }
    }
}

static void nfc_rfid_detector_view_cal_draw(Canvas* canvas, NfcRfidDetectorCalModel* model) {
    char str[16];
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "Calibration");

    // D-pad keys, labelled with how much they change the value
    const int32_t pad_x = 28, pad_y = 30;
    canvas_set_font(canvas, FontSecondary);
    nfc_rfid_detector_view_cal_arrow(canvas, pad_x, pad_y, 8, CanvasDirectionBottomToTop);
    nfc_rfid_detector_view_cal_arrow(canvas, pad_x, pad_y, 8, CanvasDirectionTopToBottom);
    nfc_rfid_detector_view_cal_arrow(canvas, pad_x, pad_y, 8, CanvasDirectionRightToLeft);
    nfc_rfid_detector_view_cal_arrow(canvas, pad_x, pad_y, 8, CanvasDirectionLeftToRight);
    canvas_draw_str_aligned(canvas, pad_x, pad_y - 10, AlignCenter, AlignBottom, "+1");
    canvas_draw_str_aligned(canvas, pad_x, pad_y + 18, AlignCenter, AlignBottom, "-1");
    canvas_draw_str_aligned(canvas, pad_x - 11, pad_y + 4, AlignRight, AlignBottom, "-0.1");
    canvas_draw_str_aligned(canvas, pad_x + 11, pad_y + 4, AlignLeft, AlignBottom, "+0.1");

    canvas_set_font(canvas, FontBigNumbers);
    snprintf(str, sizeof(str), "%+.1f", model->cal / (double)10);
    canvas_draw_str_aligned(canvas, 126, pad_y + 7, AlignRight, AlignBottom, str);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, pad_y + 17, AlignRight, AlignBottom, "ppm");

    canvas_set_font(canvas, FontSecondary);
    elements_button_center(canvas, "Save");
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "Cancel");
    canvas_draw_icon(
        canvas, 126 - canvas_string_width(canvas, "Cancel") - 13, 54, &I_Back_arrow_10x8);
}

static bool nfc_rfid_detector_view_cal_input(InputEvent* event, void* context) {
    NfcRfidDetectorCal* instance = context;

    if(event->key == InputKeyBack) {
        return false; // scene manager goes back without saving
    }
    if(event->key == InputKeyOk) {
        if(event->type == InputTypeShort && instance->callback) {
            instance->callback(NfcRfidDetectorCustomEventCalSave, instance->context);
        }
        return true;
    }
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        int16_t step = 0;
        if(event->key == InputKeyRight) step = 1;
        if(event->key == InputKeyLeft) step = -1;
        if(event->key == InputKeyUp) step = 10;
        if(event->key == InputKeyDown) step = -10;
        if(step) {
            with_view_model(
                instance->view,
                NfcRfidDetectorCalModel * model,
                {
                    if(model->cal + step >= -NFC_RFID_DETECTOR_CAL_LIMIT &&
                       model->cal + step <= NFC_RFID_DETECTOR_CAL_LIMIT) {
                        model->cal += step;
                    }
                },
                true);
        }
    }
    return true;
}

NfcRfidDetectorCal* nfc_rfid_detector_view_cal_alloc(void) {
    NfcRfidDetectorCal* instance = malloc(sizeof(NfcRfidDetectorCal));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(NfcRfidDetectorCalModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)nfc_rfid_detector_view_cal_draw);
    view_set_input_callback(instance->view, nfc_rfid_detector_view_cal_input);
    return instance;
}

void nfc_rfid_detector_view_cal_free(NfcRfidDetectorCal* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* nfc_rfid_detector_view_cal_get_view(NfcRfidDetectorCal* instance) {
    furi_assert(instance);
    return instance->view;
}

void nfc_rfid_detector_view_cal_set_callback(
    NfcRfidDetectorCal* instance,
    NfcRfidDetectorCalCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void nfc_rfid_detector_view_cal_set(NfcRfidDetectorCal* instance, int16_t cal) {
    furi_assert(instance);
    with_view_model(instance->view, NfcRfidDetectorCalModel * model, { model->cal = cal; }, true);
}

int16_t nfc_rfid_detector_view_cal_get(NfcRfidDetectorCal* instance) {
    furi_assert(instance);
    int16_t cal = 0;
    with_view_model(instance->view, NfcRfidDetectorCalModel * model, { cal = model->cal; }, false);
    return cal;
}
