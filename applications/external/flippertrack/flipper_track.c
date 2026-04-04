/**
 * FlipperTrack - HID activity simulator
 * Proof of concept for productivity monitor bypass research.
 *
 * Controls:
 *   OK          - toggle Mac / Windows mode (changes Tab modifier)
 *   Right       - toggle fake typing on / off
 *   Hold Back   - exit and restore USB
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <stdio.h>

// --- Tunable parameters ---------------------------------------------------

#define MIN_MOUSE_MS  2000UL
#define MAX_MOUSE_MS  5000UL
#define MOUSE_JITTER  20

#define MIN_CTAB_MS   45000UL
#define MAX_CTAB_MS   90000UL

#define MIN_TYPE_MS   8000UL
#define MAX_TYPE_MS   20000UL
#define TYPE_CHARS_MIN 3
#define TYPE_CHARS_MAX 8

// --------------------------------------------------------------------------

/* HID keycodes for a-z (0x04 = 'a') */
#define HID_KEY_A 0x04
#define HID_KEY_SPACE 0x2C
#define HID_KEY_BACKSPACE 0x2A

static const uint16_t LETTER_KEYS[] = {
    0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,
    0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1A,0x1B,0x1C,0x1D,
};
#define NUM_LETTERS 26

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort*         view_port;
    Gui*              gui;
    NotificationApp*  notifications;
    bool              running;
    bool              mac_mode;
    bool              typing_on;
    char              status[48];
    uint32_t          mouse_deadline;
    uint32_t          ctab_deadline;
    uint32_t          type_deadline;
} FlipperTrackApp;

// --- Drawing --------------------------------------------------------------

static void draw_callback(Canvas* canvas, void* ctx) {
    FlipperTrackApp* app = ctx;
    uint32_t now = furi_get_tick();

    canvas_clear(canvas);

    /* Title bar */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "FlipperTrack");
    canvas_draw_line(canvas, 0, 13, 127, 13);

    canvas_set_font(canvas, FontSecondary);

    /* Last action */
    canvas_draw_str(canvas, 2, 23, app->status);

    /* Mode + typing toggle on one line */
    char modeline[40];
    snprintf(modeline, sizeof(modeline), "[OK]%s  [>]Type:%s",
             app->mac_mode ? "Mac" : "Win",
             app->typing_on ? "ON" : "off");
    canvas_draw_str(canvas, 2, 33, modeline);

    /* Countdown timers */
    uint32_t ms = (app->mouse_deadline > now) ? (app->mouse_deadline - now) : 0;
    uint32_t cs = (app->ctab_deadline  > now) ? (app->ctab_deadline  - now) : 0;
    char timers[40];
    snprintf(timers, sizeof(timers), "Mouse:%lus  Tab:%lus",
             (unsigned long)(ms / 1000), (unsigned long)(cs / 1000));
    canvas_draw_str(canvas, 2, 43, timers);

    if(app->typing_on) {
        uint32_t ts = (app->type_deadline > now) ? (app->type_deadline - now) : 0;
        char tline[32];
        snprintf(tline, sizeof(tline), "Type: ~%lus", (unsigned long)(ts / 1000));
        canvas_draw_str(canvas, 2, 53, tline);
    }

    /* Exit hint at bottom */
    canvas_draw_str(canvas, 2, 63, "Hold Back: exit");
}

static void input_callback(InputEvent* event, void* ctx) {
    FlipperTrackApp* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

// --- HID helpers ----------------------------------------------------------

static void send_app_switch(bool mac_mode) {
    uint16_t key = mac_mode
        ? (KEY_MOD_LEFT_GUI  | HID_KEYBOARD_TAB)
        : (KEY_MOD_LEFT_CTRL | HID_KEYBOARD_TAB);
    furi_hal_hid_kb_press(key);
    furi_delay_ms(80);
    furi_hal_hid_kb_release(key);
    furi_delay_ms(50);
}

static void jitter_mouse(int8_t dx, int8_t dy) {
    furi_hal_hid_mouse_move(dx, dy);
    furi_delay_ms(120);
    furi_hal_hid_mouse_move(-(dx / 2), -(dy / 2));
    furi_delay_ms(60);
}

/**
 * Type N random lowercase letters then backspace them all.
 * Net text change = zero; input events generated = 2*N.
 */
static void fake_type(int n) {
    for(int i = 0; i < n; i++) {
        uint16_t key = LETTER_KEYS[rand() % NUM_LETTERS];
        furi_hal_hid_kb_press(key);
        furi_delay_ms(80 + (uint32_t)(rand() % 120));
        furi_hal_hid_kb_release(key);
        furi_delay_ms(40 + (uint32_t)(rand() % 80));
    }
    /* erase what we typed */
    for(int i = 0; i < n; i++) {
        furi_hal_hid_kb_press(HID_KEY_BACKSPACE);
        furi_delay_ms(60);
        furi_hal_hid_kb_release(HID_KEY_BACKSPACE);
        furi_delay_ms(40);
    }
}

// --- Activity bursts ------------------------------------------------------

static void do_mouse(FlipperTrackApp* app) {
    int8_t dx = (int8_t)((rand() % (MOUSE_JITTER * 2 + 1)) - MOUSE_JITTER);
    int8_t dy = (int8_t)((rand() % (MOUSE_JITTER * 2 + 1)) - MOUSE_JITTER);
    jitter_mouse(dx, dy);
    snprintf(app->status, sizeof(app->status), "Mouse (%+d,%+d)", dx, dy);
}

static void do_ctab(FlipperTrackApp* app) {
    send_app_switch(app->mac_mode);
    snprintf(app->status, sizeof(app->status), "%s+Tab sent",
             app->mac_mode ? "Cmd" : "Ctrl");
}

static void do_type(FlipperTrackApp* app) {
    int n = TYPE_CHARS_MIN + (rand() % (TYPE_CHARS_MAX - TYPE_CHARS_MIN + 1));
    fake_type(n);
    snprintf(app->status, sizeof(app->status), "Typed %d chars (+BS)", n);
}

// --- Helpers --------------------------------------------------------------

static uint32_t rand_range(uint32_t lo, uint32_t hi) {
    return lo + (uint32_t)(rand() % (int)(hi - lo));
}

// --- App lifecycle --------------------------------------------------------

int32_t flipper_track_app(void* p) {
    UNUSED(p);

    FlipperTrackApp* app = malloc(sizeof(FlipperTrackApp));
    furi_check(app);

    app->running    = true;
    app->mac_mode   = false;
    app->typing_on  = false;
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    snprintf(app->status, sizeof(app->status), "Starting...");

    /* GUI */
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* Switch USB to HID */
    FuriHalUsbInterface* usb_prev = furi_hal_usb_get_config();
    furi_hal_usb_set_config(&usb_hid, NULL);
    furi_delay_ms(1500);

    srand((unsigned int)furi_get_tick());

    uint32_t now = furi_get_tick();
    app->mouse_deadline = now + rand_range(MIN_MOUSE_MS, MAX_MOUSE_MS);
    app->ctab_deadline  = now + rand_range(MIN_CTAB_MS,  MAX_CTAB_MS);
    app->type_deadline  = now + rand_range(MIN_TYPE_MS,  MAX_TYPE_MS);

    snprintf(app->status, sizeof(app->status), "Running");
    view_port_update(app->view_port);

    while(app->running) {
        view_port_update(app->view_port);

        InputEvent event;
        FuriStatus fs = furi_message_queue_get(app->input_queue, &event, 200);
        if(fs == FuriStatusOk) {
            if(event.type == InputTypeLong && event.key == InputKeyBack) {
                app->running = false;
                break;
            }
            if(event.type == InputTypeShort) {
                if(event.key == InputKeyOk) {
                    app->mac_mode = !app->mac_mode;
                    snprintf(app->status, sizeof(app->status), "Mode: %s",
                             app->mac_mode ? "Mac (Cmd+Tab)" : "Win (Ctrl+Tab)");
                }
                if(event.key == InputKeyRight) {
                    app->typing_on = !app->typing_on;
                    snprintf(app->status, sizeof(app->status), "Typing: %s",
                             app->typing_on ? "ON" : "off");
                    /* reset deadline when enabling */
                    if(app->typing_on)
                        app->type_deadline = furi_get_tick() + rand_range(MIN_TYPE_MS, MAX_TYPE_MS);
                }
            }
        }

        now = furi_get_tick();

        if(now >= app->mouse_deadline) {
            notification_message(app->notifications, &sequence_blink_blue_10);
            do_mouse(app);
            app->mouse_deadline = now + rand_range(MIN_MOUSE_MS, MAX_MOUSE_MS);
        }

        if(now >= app->ctab_deadline) {
            do_ctab(app);
            app->ctab_deadline = now + rand_range(MIN_CTAB_MS, MAX_CTAB_MS);
        }

        if(app->typing_on && now >= app->type_deadline) {
            do_type(app);
            app->type_deadline = now + rand_range(MIN_TYPE_MS, MAX_TYPE_MS);
        }
    }

    /* Restore USB */
    furi_hal_usb_set_config(usb_prev, NULL);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->input_queue);
    free(app);

    return 0;
}
