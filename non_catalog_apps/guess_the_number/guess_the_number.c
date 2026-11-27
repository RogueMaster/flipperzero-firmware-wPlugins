/*
 * Guess The Number for Flipper Zero
 * by Prevo128
 *
 * This file is the glue between the game and the Flipper: it owns the
 * screen, the buttons, the LED/vibration and the SD card. The actual game
 * lives in gtn_game.c and the drawing in gtn_ui.c, so neither of them has
 * to know anything about the firmware.
 */

#include "gtn.h"

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

/* Stats end up in /ext/apps_data/guess_the_number/stats.bin */
#define STATS_PATH APP_DATA_PATH("stats.bin")

/* If the file is missing, broken or from an older version, we just keep
 * the fresh stats from gtn_init and start over. */
static void stats_load(GtnStats* st) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, STATS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        GtnStats tmp;
        if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp) &&
           tmp.magic == GTN_STATS_MAGIC && tmp.version == GTN_STATS_VERSION) {
            *st = tmp;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void stats_save(const GtnStats* st) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, STATS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, st, sizeof(*st));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

typedef struct {
    GtnState state;
    FuriMutex* mutex;
} GtnApp;

uint32_t gtn_random(void) {
    return furi_hal_random_get();
}

/* Milliseconds since boot. Dividing the tick instead of multiplying it
 * keeps this from overflowing after ~70 minutes of uptime. */
static uint32_t now_ms(void) {
    return furi_get_tick() / (furi_kernel_get_tick_frequency() / 1000); /* no overflow */
}

/* The GUI thread calls this whenever it redraws, so we take the lock
 * to avoid drawing a half-updated state. */
static void draw_callback(Canvas* canvas, void* ctx) {
    GtnApp* app = ctx;
    if(furi_mutex_acquire(app->mutex, 25) != FuriStatusOk) return;
    gtn_draw(canvas, &app->state, now_ms());
    furi_mutex_release(app->mutex);
}

/* Button presses just go into a queue, the main loop handles them. */
static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

int32_t guess_the_number_app(void* p) {
    UNUSED(p);

    GtnApp* app = malloc(sizeof(GtnApp));
    gtn_init(&app->state, now_ms());
    stats_load(&app->state.stats);
    GtnStats to_save;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, app);
    view_port_input_callback_set(view_port, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    /* Main loop: wait up to 40 ms for a button, then let the game tick.
     * The timeout doubles as our frame timer, so animations run at ~25 fps. */
    InputEvent event;
    bool running = true;
    while(running) {
        FuriStatus status = furi_message_queue_get(queue, &event, 40);

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        uint32_t now = now_ms();
        if(status == FuriStatusOk) {
            gtn_input(&app->state, event.key, event.type, now);
        }
        gtn_tick(&app->state, now);
        GtnFx fx = app->state.fx;
        app->state.fx = GtnFxNone;
        running = !app->state.exit;
        /* Copy the stats while we hold the lock, write them after releasing
         * it. SD writes are slow and the screen shouldn't wait for them. */
        bool save = app->state.stats_dirty || !running;
        if(save) {
            app->state.stats_dirty = false;
            to_save = app->state.stats;
        }
        furi_mutex_release(app->mutex);
        if(save) stats_save(&to_save);

        /* The game only asks for an effect, here we turn it into LED/sound. */
        switch(fx) {
        case GtnFxBlink:
            notification_message(notifications, &sequence_blink_blue_10);
            break;
        case GtnFxSuccess:
            notification_message(notifications, &sequence_success);
            break;
        case GtnFxError:
            notification_message(notifications, &sequence_error);
            break;
        default:
            break;
        }

        view_port_update(view_port);
    }

    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
