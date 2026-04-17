#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "iconedit.h"
#include "utils/file_utils.h"
#include "utils/draw.h"

#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

#include <gui/icon_i.h>

#define MAX_LINES          64
#define LINE_COUNT         4 // number of lines visible in the panel
#define END_OF_DATA_STREAM "\n" // Blank line tells script we done sending data
typedef enum {
    State_NONE,
    State_READY,
    State_FILENAME,
    State_SENDING,
    State_DONE
} SendUSBState;

typedef struct {
    IEIcon* icon;
    SendUSBState state;
    bool current_frame_only;
    char* lines[MAX_LINES];
    int num_lines;
    IconEditUpdateCallback callback;
    void* callback_context;
    SendAsType send_as;
    bool filename_prompt;
    FuriHalUsbInterface* usb_if_prev;
} SendUSBModel;
static SendUSBModel sendModel = {.state = State_NONE};

void add_line(char* line) {
    if(!line) return;
    if(sendModel.num_lines >= MAX_LINES) return;
    char* new_line = malloc(strlen(line) + 1);
    strcpy(new_line, line);
    sendModel.lines[sendModel.num_lines] = new_line;
    sendModel.num_lines++;
    if(sendModel.callback) {
        sendModel.callback(sendModel.callback_context);
    }
}

void update_line(char* line) {
    if(!line) return;
    if(sendModel.num_lines == 0) return;
    free(sendModel.lines[sendModel.num_lines - 1]);
    sendModel.lines[sendModel.num_lines - 1] = malloc(strlen(line) + 1);
    strcpy(sendModel.lines[sendModel.num_lines - 1], line);
    if(sendModel.callback) {
        sendModel.callback(sendModel.callback_context);
    }
}

void send_usb_start(IEIcon* icon, SendAsType send_as, bool current_frame_only) {
    sendModel.send_as = send_as;
    sendModel.current_frame_only = current_frame_only;
    if(send_as == SendAsC) {
        sendModel.filename_prompt = false;
    } else {
        sendModel.filename_prompt = true;
    }

    FURI_LOG_I(TAG, __FUNCTION__);
    if(!sendModel.callback) {
        FURI_LOG_E(TAG, "Callback not set for Send USB");
        return;
    }
    sendModel.icon = icon;
    for(int l = 0; l < MAX_LINES; l++) {
        if(sendModel.lines[l]) {
            free(sendModel.lines[l]);
            sendModel.lines[l] = NULL;
        }
    }
    sendModel.num_lines = 0;

    sendModel.usb_if_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_hid, NULL) == true);

    sendModel.callback(sendModel.callback_context);

    sendModel.state = State_READY;
    add_line("Ready to send?");
}

void send_usb_stop() {
    FURI_LOG_I(TAG, __FUNCTION__);
    add_line("Stopping...");
    for(int l = 0; l < MAX_LINES; l++) {
        if(sendModel.lines[l]) {
            free(sendModel.lines[l]);
            sendModel.lines[l] = NULL;
        }
    }
    sendModel.num_lines = 0;

    furi_hal_usb_set_config(sendModel.usb_if_prev, NULL);
    sendModel.state = State_NONE;
}

void send_usb_send_str(const char* str) {
    for(size_t i = 0, len = strlen(str); i < len; i++) {
        uint16_t key = HID_ASCII_TO_KEY(str[i]);
        furi_hal_hid_kb_press(key);
        furi_hal_hid_kb_release(key);
    }
}

void send_usb_set_update_callback(IconEditUpdateCallback callback, void* context) {
    sendModel.callback = callback;
    sendModel.callback_context = context;
}

void send_usb_send_filename() {
    FuriString* filename = furi_string_alloc_set(sendModel.icon->name);
    if(sendModel.send_as == SendAsPNG) {
        // Anim's only need base name as our receive script will generate full filenames
        if(sendModel.current_frame_only) {
            furi_string_cat_str(filename, ".png");
        }
    } else if(sendModel.send_as == SendAsBMX) {
        furi_string_cat_str(filename, ".bmx");
    }
    furi_string_cat_str(filename, "\n");
    send_usb_send_str(furi_string_get_cstr(filename));
    send_usb_send_str(END_OF_DATA_STREAM);
    furi_string_free(filename);

    add_line("Ready to send data?");
    sendModel.state = State_READY;
    sendModel.filename_prompt = false;
}

void send_usb_send_icon() {
    FURI_LOG_I(TAG, __FUNCTION__);
    add_line("Sending...");
    FuriString* icon_text = NULL;
    switch(sendModel.send_as) {
    case SendAsC:
        // For .C files, sending a single frame or the whole animation is still a single "file"
        icon_text = c_file_generate(sendModel.icon, sendModel.current_frame_only);
        send_usb_send_str(furi_string_get_cstr(icon_text));
        send_usb_send_str(END_OF_DATA_STREAM);
        furi_string_free(icon_text);
        break;
    case SendAsPNG:
        // For PNG files, we'll need to generate text for each frame, and the recipient script will
        // save each as a separate file. When the script receives the text "frame_rate", it knows
        // we done with the frames and will then receive the frame rate itself, and save.
        if(sendModel.current_frame_only) {
            icon_text = png_file_generate_frame(sendModel.icon, sendModel.icon->current_frame);
            send_usb_send_str(furi_string_get_cstr(icon_text));
            send_usb_send_str(END_OF_DATA_STREAM);
            furi_string_free(icon_text);
        } else {
            for(size_t f = 0; f < sendModel.icon->frame_count; f++) {
                char progress[32];
                snprintf(
                    progress,
                    sizeof(progress),
                    "Sending: %d/%d",
                    f + 1,
                    sendModel.icon->frame_count);
                update_line(progress);
                icon_text = png_file_generate_frame(sendModel.icon, f);
                send_usb_send_str(furi_string_get_cstr(icon_text));
                send_usb_send_str(END_OF_DATA_STREAM);
                furi_string_free(icon_text);
            }
            send_usb_send_str("frame_rate\n");
            send_usb_send_str(END_OF_DATA_STREAM);
            char fr_buf[8];
            snprintf(fr_buf, sizeof(fr_buf), "%d\n", sendModel.icon->frame_rate);
            send_usb_send_str(fr_buf);
            send_usb_send_str(END_OF_DATA_STREAM);
        }
        break;
    case SendAsBMX:
        // No animation support yet, so current_frame is 0
        icon_text = bmx_file_generate_frame(sendModel.icon, sendModel.icon->current_frame);
        send_usb_send_str(furi_string_get_cstr(icon_text));
        send_usb_send_str(END_OF_DATA_STREAM);
        furi_string_free(icon_text);
        break;
    default:
        break;
    }

    furi_hal_hid_kb_release_all();

    add_line("Sent!");
    sendModel.state = State_DONE;
}

void send_usb_draw(Canvas* canvas, void* context) {
    FURI_LOG_I(TAG, __FUNCTION__);
    UNUSED(context);

    // draw a large panel that will contain (maybe scrolling) text
    // showing the current status of the send
    // also maybe pause at certain actions, prompting user to OK to continue

    const int x = 20; // redo this to use canvas_width, etc
    const int y = 10;
    const int pad = 2; // outside padding between frame and line
    const int line_h = 7;
    const int line_w = 90;
    const int line_pad = 2; // the space between

    // draw an empty panel frame
    int rw = line_w + (pad * 2) + (line_pad * 2);
    int rh = (line_h + (line_pad * 2)) * LINE_COUNT + (pad * 2);
    ie_draw_modal_panel_frame(canvas, x, y, rw, rh);

    int start = sendModel.num_lines <= LINE_COUNT ? 0 : sendModel.num_lines - LINE_COUNT;
    int end = sendModel.num_lines <= LINE_COUNT ? sendModel.num_lines : start + LINE_COUNT;

    for(int l = start; l < end; l++) {
        FURI_LOG_I(TAG, "usb: %s", sendModel.lines[l]);
        canvas_draw_str_aligned(
            canvas,
            x + pad + line_pad + 1,
            y + pad + line_pad + (l - start) * (line_h + line_pad * 2),
            AlignLeft,
            AlignTop,
            sendModel.lines[l]);
    }
    // draw a scrollbar / scroll indicator?
}

bool send_usb_input(InputEvent* event, void* context) {
    FURI_LOG_I(TAG, __FUNCTION__);
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyBack: {
            consumed = false; // send control back to File panel

            app->panel = Panel_File;
            send_usb_stop();
            break;
        }
        case InputKeyOk: {
            switch(sendModel.state) {
            case State_READY:
                // Our USB connection is ready and we're prompting user
                // if they are ready to transmit data
                if(!sendModel.filename_prompt) {
                    sendModel.state = State_SENDING;
                    send_usb_send_icon();
                } else {
                    sendModel.state = State_FILENAME;
                    add_line("Send filename?");
                }
                break;
            case State_FILENAME:
                send_usb_send_filename();
                break;
            case State_DONE:
                app->panel = Panel_File;
                send_usb_stop();
                consumed = false;
                break;
            default:
                break;
            }
            break;
        }
        case InputKeyRight: {
            // user wants to skip sending filename
            sendModel.filename_prompt = false;
            sendModel.state = State_SENDING;
            send_usb_send_icon();
            break;
        }
        default:
            break;
        }
    }
    return consumed;
}
