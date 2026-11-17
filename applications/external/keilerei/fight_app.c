/* KEILEREI – Flipper-Zero-App (Plattform-Schicht). Spiellogik: fight_core.c
 * Steuerung: Steuerkreuz = bewegen, OK = schlagen (halten = Dauerfeuer),
 *            Zurueck kurz = Pause (im Titel: beenden), Zurueck lang = beenden. */
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include "fight_core.h"

#define HI_PATH APP_DATA_PATH("hiscore.bin")

typedef enum {
    EvTick,
    EvKey
} EvType;
typedef struct {
    EvType type;
    InputEvent input;
} AppEvent;

typedef struct {
    Fight game;
    uint8_t fb[FG_FB_SIZE];
    uint8_t held, pressed;
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    NotificationApp* notif;
} App;

static const NotificationSequence seq_hit =
    {&message_note_c6, &message_delay_10, &message_sound_off, NULL};
static const NotificationSequence seq_pick = {
    &message_note_g5,
    &message_delay_25,
    &message_note_c6,
    &message_delay_25,
    &message_sound_off,
    NULL};
static const NotificationSequence seq_hurt = {
    &message_vibro_on,
    &message_note_c3,
    &message_delay_50,
    &message_vibro_off,
    &message_sound_off,
    NULL};
static const NotificationSequence seq_wave = {
    &message_note_c4,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    NULL};

static uint32_t hi_load(void) {
    uint32_t hi = 0;
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, HI_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(f, &hi, sizeof(hi)) != sizeof(hi)) hi = 0;
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return hi;
}
static void hi_save(uint32_t hi) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, HI_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(f, &hi, sizeof(hi));
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void draw_cb(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    fight_render(&app->game, app->fb);
    canvas_draw_xbm(canvas, 0, 0, 128, 64, app->fb);
    furi_mutex_release(app->mutex);
}
static void input_cb(InputEvent* ev, void* ctx) {
    App* app = ctx;
    AppEvent e = {.type = EvKey, .input = *ev};
    furi_message_queue_put(app->queue, &e, 0);
}
static void timer_cb(void* ctx) {
    App* app = ctx;
    AppEvent e = {.type = EvTick};
    furi_message_queue_put(app->queue, &e, 0);
}
static uint8_t key_bit(InputKey k) {
    switch(k) {
    case InputKeyUp:
        return K_UP;
    case InputKeyRight:
        return K_RIGHT;
    case InputKeyDown:
        return K_DOWN;
    case InputKeyLeft:
        return K_LEFT;
    case InputKeyOk:
        return K_A;
    default:
        return 0;
    }
}

int32_t keilerei_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    fight_init(&app->game, furi_get_tick() ^ 0x9E3779B9u, hi_load());
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(16, sizeof(AppEvent));
    app->notif = furi_record_open(RECORD_NOTIFICATION);
    notification_message(app->notif, &sequence_display_backlight_enforce_on);

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, app);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    FuriTimer* timer = furi_timer_alloc(timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(timer, furi_ms_to_ticks(50)); /* 20 Logik-Ticks/s wie im Web */

    bool run = true;
    AppEvent ev;
    while(run) {
        if(furi_message_queue_get(app->queue, &ev, FuriWaitForever) != FuriStatusOk) continue;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(ev.type == EvKey) {
            InputEvent* in = &ev.input;
            if(in->key == InputKeyBack) {
                if(in->type == InputTypeLong)
                    run = false;
                else if(in->type == InputTypeShort) {
                    if(app->game.st == S_TITLE)
                        run = false;
                    else
                        app->pressed |= K_B;
                }
            } else {
                uint8_t b = key_bit(in->key);
                if(in->type == InputTypePress) {
                    if(!(app->held & b)) app->pressed |= b;
                    app->held |= b;
                } else if(in->type == InputTypeRelease)
                    app->held &= (uint8_t)~b;
            }
        } else {
            fight_step(&app->game, app->held, app->pressed);
            app->pressed = 0;
            int evt = app->game.evt;
            if(evt & EV_OVER) hi_save(app->game.hi);
            if(evt & (EV_HURT | EV_OVER))
                notification_message(app->notif, &seq_hurt);
            else if(evt & EV_WAVE)
                notification_message(app->notif, &seq_wave);
            else if(evt & EV_PICK)
                notification_message(app->notif, &seq_pick);
            else if(evt & (EV_KILL | EV_HIT))
                notification_message(app->notif, &seq_hit);
        }
        furi_mutex_release(app->mutex);
        if(ev.type == EvTick) view_port_update(vp);
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);
    hi_save(app->game.hi);
    notification_message(app->notif, &sequence_display_backlight_enforce_auto);
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
