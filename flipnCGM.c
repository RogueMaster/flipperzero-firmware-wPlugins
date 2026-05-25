#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <string.h>
#include <stdio.h>

// 32-char alphabet for FreeStyle Libre serial decoding.
// B, I, O, S omitted to avoid visual ambiguity with 8, 1, 0, 5.
static const char SERIAL_LOOKUP[32] = "0123456789ACDEFGHJKLMNPQRTUVWXYZ";

// Brief two-note ascending chime (~200ms total) on successful Libre read.
static const NotificationSequence sequence_cgm_success = {
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

// Single short low beep (~100ms) when a non-Libre tag is scanned.
static const NotificationSequence sequence_cgm_error = {
    &message_note_c3,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

typedef enum {
    AppStateScanning,
    AppStateResult,
    AppStateNotALibre,
} AppState;

typedef struct {
    AppState state;
    char serial[10];       // 9-char serial + null terminator
    char uid_str[32];      // "E0:7A:xx:xx:xx:xx:xx:xx\0"
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
    Nfc* nfc;
    NfcPoller* poller;
    FuriMutex* mutex;
    volatile bool poller_running; // true while poller is active
} App;

// Decode a FreeStyle Libre NFC UID to its 9-character ASCII serial number.
// uid must be 8 bytes MSB-first (uid[0]==0xE0, uid[1]==0x7A).
// Algorithm: treat uid[2..7] as a 48-bit big-endian integer, extract
// 9 groups of 5 bits MSB-first, look each up in SERIAL_LOOKUP.
static void decode_libre_serial(const uint8_t* uid, char* out) {
    uint64_t value = 0;
    for(int i = 2; i <= 7; i++) {
        value = (value << 8) | (uint64_t)uid[i];
    }
    for(int i = 8; i >= 0; i--) {
        out[8 - i] = SERIAL_LOOKUP[(value >> (i * 5)) & 0x1F];
    }
    out[9] = '\0';
}

static void draw_callback(Canvas* canvas, void* context) {
    App* app = (App*)context;
    // Use a short timeout so the GUI thread never blocks indefinitely if the
    // poller thread holds the mutex during an NFC transaction.
    if(furi_mutex_acquire(app->mutex, 25) != FuriStatusOk) return;

    canvas_clear(canvas);

    switch(app->state) {
    case AppStateScanning:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignCenter, "flipnCGM");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "Hold a FreeStyle Libre");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "sensor to the back");
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignCenter, "of your Flipper");
        break;

    case AppStateResult:
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "FreeStyle Libre Serial:");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, app->serial);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, app->uid_str);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "OK: scan again  Back: exit");
        break;

    case AppStateNotALibre:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, "Not a Libre sensor");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Tap a FreeStyle Libre CGM");
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "OK: try again  Back: exit");
        break;
    }

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* input_event, void* context) {
    App* app = (App*)context;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

static NfcCommand poller_callback(NfcGenericEvent event, void* context) {
    App* app = (App*)context;
    Iso15693_3PollerEvent* poller_event = (Iso15693_3PollerEvent*)event.event_data;

    if(poller_event->type == Iso15693_3PollerEventTypeReady) {
        const Iso15693_3Data* iso_data =
            (const Iso15693_3Data*)nfc_poller_get_data(app->poller);
        const uint8_t* uid = iso_data->uid;

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(uid[0] == 0xE0 && uid[1] == 0x7A) {
            decode_libre_serial(uid, app->serial);
            snprintf(
                app->uid_str,
                sizeof(app->uid_str),
                "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                uid[0], uid[1], uid[2], uid[3],
                uid[4], uid[5], uid[6], uid[7]);
            app->state = AppStateResult;
            notification_message(app->notifications, &sequence_cgm_success);
        } else {
            app->state = AppStateNotALibre;
            notification_message(app->notifications, &sequence_cgm_error);
        }
        // Mark stopped before returning NfcCommandStop so cleanup knows
        // not to call nfc_poller_stop() on an already-stopped poller.
        app->poller_running = false;
        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
        // Stop after one read — avoids a tight reset loop when the tag
        // stays in range, which can starve the input queue.
        // The user presses OK to scan again.
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

int32_t flipncgm_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    furi_assert(app);
    memset(app, 0, sizeof(App));
    app->state = AppStateScanning;
    app->poller_running = true;

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->nfc = nfc_alloc();
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
    nfc_poller_start(app->poller, poller_callback, app);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            // Handle both short and long Back presses so a held Back always exits.
            if(event.key == InputKeyBack &&
               (event.type == InputTypeShort || event.type == InputTypeLong)) {
                running = false;
            } else if(event.key == InputKeyOk && event.type == InputTypeShort) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                if(app->state != AppStateScanning) {
                    app->state = AppStateScanning;
                    app->poller_running = true;
                    furi_mutex_release(app->mutex);
                    // Restart the poller for the next scan.
                    nfc_poller_start(app->poller, poller_callback, app);
                } else {
                    furi_mutex_release(app->mutex);
                }
                view_port_update(app->view_port);
            }
        }
    }

    // Only stop the poller if it is still running — calling nfc_poller_stop()
    // on an already-stopped poller is undefined behaviour and may crash.
    if(app->poller_running) {
        nfc_poller_stop(app->poller);
    }
    nfc_poller_free(app->poller);
    nfc_free(app->nfc);

    furi_record_close(RECORD_NOTIFICATION);

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);

    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
