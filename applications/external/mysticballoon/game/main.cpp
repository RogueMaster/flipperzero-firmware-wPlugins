/**
  @file   main.cpp
  @author apfxtech
  @brief  application shell: viewport, input, timing, sound and saves

  Mystic Balloon, Flipper Zero port

  apfxtech, 2026, license: MIT (see LICENSE)

  SPDX-License-Identifier: MIT
*/

#include <furi.h>
#include <furi_hal_rtc.h>
#include <furi_hal_speaker.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

#include <string.h>

#include "mystic_balloon.h"
#include "render.h"

#define MYBL_FRAME_RATE  60
#define MYBL_TONE_VOLUME 1.0f
#define MYBL_SAVE_PATH   APP_DATA_PATH("eeprom.bin")

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;

    uint8_t held;
    uint8_t press_latch;
    bool exit_requested;

    bool speaker_owned;
    bool tone_active;
    uint32_t tone_until;

    MyblSave save;
} MyblApp;

static MyblApp* app_instance = NULL;

void platform_tone(uint16_t frequency, uint16_t duration_ms) {
    MyblApp* app = app_instance;
    if(!app || !app->speaker_owned || frequency == 0) return;

    furi_hal_speaker_start((float)frequency, MYBL_TONE_VOLUME);
    app->tone_active = true;
    app->tone_until = furi_get_tick() + (furi_kernel_get_tick_frequency() * duration_ms) / 1000;
}

static void tone_expire(MyblApp* app) {
    if(!app->tone_active) return;
    if((int32_t)(furi_get_tick() - app->tone_until) < 0) return;

    furi_hal_speaker_stop();
    app->tone_active = false;
}

static void save_load(MyblSave* save) {
    Storage* storage = (Storage*)furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, MYBL_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        MyblSave stored;
        if(storage_file_read(file, &stored, sizeof(stored)) == sizeof(stored)) {
            *save = stored;
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void save_store(const MyblSave* save) {
    Storage* storage = (Storage*)furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, MYBL_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, save, sizeof(*save));
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static uint8_t button_from_key(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return MYBL_UP;
    case InputKeyDown:
        return MYBL_DOWN;
    case InputKeyLeft:
        return MYBL_LEFT;
    case InputKeyRight:
        return MYBL_RIGHT;
    case InputKeyOk:
        return MYBL_OK;
    case InputKeyBack:
        return MYBL_BACK;
    default:
        return 0;
    }
}

static void input_apply(MyblApp* app, const InputEvent* event) {
    const uint8_t bit = button_from_key(event->key);
    if(!bit) return;

    if(event->type == InputTypePress) {
        app->held |= bit;
        app->press_latch |= bit;
    } else if(event->type == InputTypeRepeat) {
        app->held |= bit;
    } else if(event->type == InputTypeRelease) {
        app->held &= (uint8_t)~bit;
    }
}

static void frame_advance(MyblApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    const uint8_t press = app->press_latch;
    app->press_latch = 0;
    mybl_frame(app->held, press);

    if(mybl_exit_requested()) app->exit_requested = true;

    furi_mutex_release(app->mutex);

    if(mybl_take_save(&app->save)) {
        save_store(&app->save);
    }
}

static void draw_callback(Canvas* canvas, void* context) {
    MyblApp* app = (MyblApp*)context;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    gfx_present(canvas);
    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* context) {
    MyblApp* app = (MyblApp*)context;
    furi_message_queue_put(app->input_queue, event, 0);
}

extern "C" int32_t mybl_app(void* p) {
    UNUSED(p);

    MyblApp* app = (MyblApp*)malloc(sizeof(MyblApp));
    memset(app, 0, sizeof(MyblApp));
    app_instance = app;

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(16, sizeof(InputEvent));

    app->speaker_owned = furi_hal_speaker_acquire(100);

    save_load(&app->save);
    mybl_start(!furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode), &app->save);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = (Gui*)furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    const uint32_t period = furi_kernel_get_tick_frequency() / MYBL_FRAME_RATE;
    uint32_t next_frame = furi_get_tick() + period;

    while(!app->exit_requested) {
        const uint32_t now = furi_get_tick();
        const int32_t remaining = (int32_t)(next_frame - now);

        InputEvent event;
        if(remaining > 0 &&
           furi_message_queue_get(app->input_queue, &event, (uint32_t)remaining) == FuriStatusOk) {
            input_apply(app, &event);
            continue;
        }

        next_frame += period;
        if((int32_t)(furi_get_tick() - next_frame) > (int32_t)period) {
            next_frame = furi_get_tick() + period;
        }

        tone_expire(app);
        frame_advance(app);
        view_port_update(app->view_port);
    }

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);

    if(app->speaker_owned) {
        if(app->tone_active) furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    if(mybl_take_save(&app->save)) {
        save_store(&app->save);
    }

    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);

    app_instance = NULL;
    free(app);

    return 0;
}
