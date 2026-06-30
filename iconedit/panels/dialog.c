#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"
#include "../utils/draw.h"
#include "../utils/notification.h"

#define LIST_LEN(x) (sizeof(x) / sizeof((x)[0]))

const DialogButton _BtnList_OK[] = {DialogBtn_OK};
const DialogButton _BtnList_OK_CANCEL[] = {DialogBtn_CANCEL, DialogBtn_OK};

const DialogButton* Dialog_PromptBtns[] = {
    _BtnList_OK,
    _BtnList_OK_CANCEL,
};
const int Dialog_PromptBtns_Count[] = {
    LIST_LEN(_BtnList_OK),
    LIST_LEN(_BtnList_OK_CANCEL),
};

const char* Dialog_BtnText[3] = {
    [DialogBtn_NONE] = "*none*",
    [DialogBtn_OK] = "OK",
    [DialogBtn_CANCEL] = "Cancel",
};

static struct {
    FuriString* message;
    DialogPrompt prompt;
    DialogCallback callback;
    void* context;

    int button; // the currently / pressed button index
    PanelType called_from;
} dialogModel = {.message = NULL, .prompt = Dialog_OK, .button = DialogBtn_NONE};

void dialog_setup(const char* msg, DialogPrompt prompt, DialogCallback callback, void* context) {
    // setup message text
    if(dialogModel.message == NULL) {
        dialogModel.message = furi_string_alloc();
    }
    furi_string_set_str(dialogModel.message, msg);

    // setup prompt
    dialogModel.prompt = prompt;
    // set the default button index to the position of OK for this prompt
    dialogModel.button = -1;
    const DialogButton* list = Dialog_PromptBtns[prompt];
    for(int index = 0; index < Dialog_PromptBtns_Count[dialogModel.prompt]; index++) {
        if(list[index] == DialogBtn_OK) {
            dialogModel.button = index;
            break;
        }
    }
    assert(dialogModel.button != -1); // this is bad

    dialogModel.callback = callback;
    dialogModel.context = context;
}

void dialog_free_dialog() {
    if(dialogModel.message) {
        furi_string_free(dialogModel.message);
        dialogModel.message = NULL;
    }
}

void dialog_return_to_panel(void* context, DialogButton button) {
    UNUSED(button);
    IconEdit* app = context;
    app->panel = dialogModel.called_from;
}
void dialog_info_dialog(IconEdit* app, const char* msg, PanelType return_to) {
    dialogModel.called_from = return_to;
    app->panel = Panel_Dialog;
    dialog_setup(msg, Dialog_OK, dialog_return_to_panel, app);
}

DialogButton dialog_get_button() {
    if(dialogModel.button == -1) {
        assert(false);
        return DialogBtn_NONE;
    }
    assert(dialogModel.button < Dialog_PromptBtns_Count[dialogModel.prompt]);
    const DialogButton* list = Dialog_PromptBtns[dialogModel.prompt];
    return list[dialogModel.button];
}

void dialog_draw(Canvas* canvas, void* context) {
    UNUSED(context);

    if(dialogModel.message == NULL) {
        dialogModel.message = furi_string_alloc_set_str("No message!");
    }

    // padding between border and content
    int xpad = 4;
    int ypad = 3;

    int rw = canvas_string_width(canvas, furi_string_get_cstr(dialogModel.message)) + xpad * 2;
    // assume text is one line only (for now) - 10px high?
    // room for buttons - 15px ok?
    int rh = 10 + 15 + ypad * 2;
    int x = (canvas_width(canvas) - rw) / 2;
    int y = (canvas_height(canvas) - rh) / 2;

    ie_draw_modal_panel_frame(canvas, x, y, rw, rh);

    // message
    canvas_draw_str_aligned(
        canvas,
        canvas_width(canvas) / 2,
        y + ypad,
        AlignCenter,
        AlignTop,
        furi_string_get_cstr(dialogModel.message));

    // buttons will be centered
    // compute starting x offset
    int num_btn = Dialog_PromptBtns_Count[dialogModel.prompt];
    int btn_width = 32;
    int btn_pad = 3; // space between btns
    int xo = (canvas_width(canvas) - ((num_btn * btn_width) + (num_btn - 1) * btn_pad)) / 2;
    int yo = (y + rh) - ypad - 12;

    for(int b = 0; b < num_btn; b++) {
        DialogButton btn = Dialog_PromptBtns[dialogModel.prompt][b];
        canvas_draw_str_aligned(
            canvas,
            xo + (b * (btn_width + btn_pad)) + btn_width / 2,
            yo + 2,
            AlignCenter,
            AlignTop,
            Dialog_BtnText[btn]);

        if(dialogModel.button == b) {
            canvas_set_color(canvas, ColorXOR);
            canvas_draw_rbox(canvas, xo + (b * (btn_width + btn_pad)), yo, btn_width, 12, 0);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_rframe(canvas, xo + (b * (btn_width + btn_pad)), yo, btn_width, 12, 1);
        }
    }
}

bool dialog_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    UNUSED(app);
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyLeft: {
            dialogModel.button -= dialogModel.button > 0;
            break;
        }
        case InputKeyRight: {
            dialogModel.button += dialogModel.button <
                                  Dialog_PromptBtns_Count[dialogModel.prompt] - 1;
            break;
        }
        case InputKeyOk:
            dialogModel.callback(
                dialogModel.context, Dialog_PromptBtns[dialogModel.prompt][dialogModel.button]);
            furi_string_free(dialogModel.message);
            dialogModel.message = NULL;
            break;
        // case InputKeyBack:
        //     // bail - discard new file dimensions
        //     app->panel = Panel_File;
        //     break;
        default:
            break;
        }
    }
    return consumed;
}
