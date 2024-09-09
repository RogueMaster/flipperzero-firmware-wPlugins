#include <dialogs/dialogs.h>

#include <mp_flipper_modflipperzero.h>
#include <mp_flipper_runtime.h>

#include "mp_flipper_context.h"

void mp_flipper_dialog_message_set_text(
    const char* text,
    uint8_t x,
    uint8_t y,
    uint8_t h,
    uint8_t v) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    Align align_x = h == MP_FLIPPER_ALIGN_BEGIN ? AlignLeft : AlignRight;
    Align align_y = v == MP_FLIPPER_ALIGN_BEGIN ? AlignTop : AlignBottom;

    align_x = h == MP_FLIPPER_ALIGN_CENTER ? AlignCenter : align_x;
    align_y = v == MP_FLIPPER_ALIGN_CENTER ? AlignCenter : align_y;

    dialog_message_set_text(ctx->dialog_message, text, x, y, align_x, align_y);
}

void mp_flipper_dialog_message_set_header(
    const char* text,
    uint8_t x,
    uint8_t y,
    uint8_t h,
    uint8_t v) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    Align align_x = h == MP_FLIPPER_ALIGN_BEGIN ? AlignLeft : AlignRight;
    Align align_y = v == MP_FLIPPER_ALIGN_BEGIN ? AlignTop : AlignBottom;

    align_x = h == MP_FLIPPER_ALIGN_CENTER ? AlignCenter : align_x;
    align_y = v == MP_FLIPPER_ALIGN_CENTER ? AlignCenter : align_y;

    dialog_message_set_header(ctx->dialog_message, text, x, y, align_x, align_y);
}

void mp_flipper_dialog_message_set_button(const char* text, uint8_t button) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    // left button
    if(button == MP_FLIPPER_INPUT_BUTTON_LEFT) {
        ctx->dialog_message_button_left = text;
    }
    // center button
    else if(button == MP_FLIPPER_INPUT_BUTTON_OK) {
        ctx->dialog_message_button_center = text;
    }
    // right button
    else if(button == MP_FLIPPER_INPUT_BUTTON_RIGHT) {
        ctx->dialog_message_button_right = text;
    }

    dialog_message_set_buttons(
        ctx->dialog_message,
        ctx->dialog_message_button_left,
        ctx->dialog_message_button_center,
        ctx->dialog_message_button_right);
}

uint8_t mp_flipper_dialog_message_show() {
    mp_flipper_context_t* ctx = mp_flipper_context;

    gui_direct_draw_release(ctx->gui);

    DialogsApp* dialog = furi_record_open(RECORD_DIALOGS);

    uint8_t button = dialog_message_show(dialog, ctx->dialog_message);

    furi_record_close(RECORD_DIALOGS);

    ctx->canvas = gui_direct_draw_acquire(ctx->gui);

    switch(button) {
    case DialogMessageButtonLeft:
        return MP_FLIPPER_INPUT_BUTTON_LEFT;
    case DialogMessageButtonCenter:
        return MP_FLIPPER_INPUT_BUTTON_OK;
    case DialogMessageButtonRight:
        return MP_FLIPPER_INPUT_BUTTON_RIGHT;
    case DialogMessageButtonBack:
    default:
        return MP_FLIPPER_INPUT_BUTTON_BACK;
    }
}

void mp_flipper_dialog_message_clear() {
    mp_flipper_context_t* ctx = mp_flipper_context;

    dialog_message_free(ctx->dialog_message);

    ctx->dialog_message = dialog_message_alloc();

    ctx->dialog_message_button_left = NULL;
    ctx->dialog_message_button_center = NULL;
    ctx->dialog_message_button_right = NULL;
}
