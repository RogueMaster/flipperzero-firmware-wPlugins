#include <malloc.h>

#include <furi.h>
#include <gui/gui.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <cli/cli.h>
#include <cli/cli_vcp.h>

#include <mp_flipper_runtime.h>
#include <mp_flipper_compiler.h>

#include "upython.h"
#include "upython_icons.h"

static void on_input(const void* event, void* ctx) {
    UNUSED(ctx);

    InputKey key = ((InputEvent*)event)->key;
    InputType type = ((InputEvent*)event)->type;

    if(type != InputTypeRelease) {
        return;
    }

    switch(key) {
    case InputKeyOk:
        action = ActionOpen;
        break;
    case InputKeyBack:
        action = ActionExit;
        break;
    default:
        action = ActionNone;
        break;
    }
}

bool upython_confirm_exit_action() {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    DialogMessage* message = dialog_message_alloc();

    dialog_message_set_text(message, "Close uPython?", 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "Yes", NULL, "No");

    DialogMessageButton button = dialog_message_show(dialogs, message);

    dialog_message_free(message);

    furi_record_close(RECORD_DIALOGS);

    return button == DialogMessageButtonLeft;
}

bool upython_select_python_file(FuriString* file_path) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    DialogsFileBrowserOptions browser_options;

    dialog_file_browser_set_basic_options(&browser_options, "py", NULL);

    browser_options.hide_ext = false;
    browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;

    bool result = dialog_file_browser_show(dialogs, file_path, file_path, &browser_options);

    furi_record_close(RECORD_DIALOGS);

    return result;
}

Action upython_splash_screen() {
    if(action != ActionNone) {
        return action;
    }

    Gui* gui = furi_record_open(RECORD_GUI);
    FuriPubSub* input_event_queue = furi_record_open(RECORD_INPUT_EVENTS);
    FuriPubSubSubscription* input_event = furi_pubsub_subscribe(input_event_queue, on_input, NULL);

    Canvas* canvas = gui_direct_draw_acquire(gui);

    canvas_draw_icon(canvas, 0, 0, &I_splash);
    canvas_draw_icon(canvas, 82, 17, &I_qrcode);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 66, 3, AlignLeft, AlignTop, "Micro");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 90, 2, AlignLeft, AlignTop, "Python");

    canvas_set_font(canvas, FontSecondary);

    canvas_draw_icon(canvas, 65, 53, &I_Pin_back_arrow_10x8);
    canvas_draw_str_aligned(canvas, 78, 54, AlignLeft, AlignTop, "Exit");

    canvas_draw_icon(canvas, 98, 54, &I_ButtonCenter_7x7);
    canvas_draw_str_aligned(canvas, 107, 54, AlignLeft, AlignTop, "Open");

    canvas_commit(canvas);

    while(action == ActionNone) {
        furi_delay_ms(1);
    }

    furi_pubsub_unsubscribe(input_event_queue, input_event);

    gui_direct_draw_release(gui);

    furi_record_close(RECORD_INPUT_EVENTS);
    furi_record_close(RECORD_GUI);

    return action;
}
