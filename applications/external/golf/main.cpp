#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <new>

#include "game/game.hpp"
#include "lib/Arduboy2.h"
#include "lib/ArduboyTones.h"
#include "lib/EEPROM.h"
//#include "lib/ArduboyFX.h"

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64
#define FB_SIZE        (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)

FuriMessageQueue* g_arduboy_sound_queue = NULL;
FuriThread* g_arduboy_sound_thread = NULL;
volatile bool g_arduboy_sound_thread_running = false;
volatile bool g_arduboy_audio_enabled = false;
volatile bool g_arduboy_tones_playing = false;
volatile bool g_arduboy_force_high = false;
volatile bool g_arduboy_force_norm = false;
volatile uint8_t g_arduboy_volume_mode = VOLUME_IN_TONE;

uint16_t rand_seed = 1;

static Arduboy2Base a;

static uint8_t g_framebuffer[FB_SIZE];
uint8_t* buf = g_framebuffer;

static bool (*g_audio_enabled_cb)() = NULL;
static bool audio_enabled_cb_impl() {
    return a.audio.enabled();
}
ArduboyTones sound((bool (*)())0);

typedef struct {
    Gui* gui;
    Canvas* canvas;
    FuriMutex* mutex;
    FuriMutex* fb_mutex;

    FuriPubSub* input_events;
    FuriPubSubSubscription* input_sub;

    uint8_t front_buffer[FB_SIZE];

    volatile uint8_t input_state;
    volatile bool exit_requested;
    volatile bool shutting_down;
} FlipperState;

static FlipperState* g_state = NULL;

static FuriMessageQueue* s_input_queue = NULL;
static volatile uint32_t s_input_cb_inflight = 0;
static volatile uint32_t s_fb_cb_inflight = 0;

static inline void wait_inflight_zero(volatile uint32_t* counter) {
    while(__atomic_load_n(counter, __ATOMIC_ACQUIRE) != 0) {
        furi_delay_ms(1);
    }
}

static void framebuffer_commit_callback(
    uint8_t* data,
    size_t size,
    CanvasOrientation orientation,
    void* context) {
    __atomic_fetch_add(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);

    (void)orientation;
    FlipperState* st = (FlipperState*)context;

    if(!st || !data || size < FB_SIZE || st->shutting_down) {
        __atomic_fetch_sub(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);
        return;
    }

    if(furi_mutex_acquire(st->fb_mutex, 0) != FuriStatusOk) {
        __atomic_fetch_sub(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);
        return;
    }

    const uint8_t* src = st->front_buffer;
    for(size_t i = 0; i < FB_SIZE; i++) {
        data[i] = (uint8_t)(src[i] ^ 0xFF);
    }

    furi_mutex_release(st->fb_mutex);

    __atomic_fetch_sub(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);
}

static void input_events_callback(const void* value, void* ctx) {
    (void)ctx;

    __atomic_fetch_add(&s_input_cb_inflight, 1, __ATOMIC_RELAXED);

    if(value) {
        FuriMessageQueue* q = __atomic_load_n(&s_input_queue, __ATOMIC_ACQUIRE);
        if(q) {
            InputEvent ev = *(const InputEvent*)value;
            (void)furi_message_queue_put(q, &ev, 0);
        }
    }

    __atomic_fetch_sub(&s_input_cb_inflight, 1, __ATOMIC_RELAXED);
}

void save_audio_on_off() {
    a.audio.saveOnOff();
    g_arduboy_audio_enabled = a.audio.enabled();
}

void toggle_audio() {
    if(a.audio.enabled()) {
        a.audio.off();
        sound.noTone();
    } else {
        a.audio.on();
    }
    a.audio.saveOnOff();
    g_arduboy_audio_enabled = a.audio.enabled();
}

bool audio_enabled() {
    return a.audio.enabled();
}

uint16_t time_ms() {
    return (uint16_t)millis();
}

uint8_t poll_btns() {
    a.pollButtons();

    uint8_t out = 0;
    if(a.pressed(UP_BUTTON)) out |= BTN_UP;
    if(a.pressed(DOWN_BUTTON)) out |= BTN_DOWN;
    if(a.pressed(LEFT_BUTTON)) out |= BTN_LEFT;
    if(a.pressed(RIGHT_BUTTON)) out |= BTN_RIGHT;

    if(a.pressed(A_BUTTON)) out |= BTN_B;
    if(a.pressed(B_BUTTON)) out |= BTN_A;

    return out;
}

extern "C" int32_t arduboy_app(void* p) {
    (void)p;

    Gui* gui = NULL;
    Canvas* canvas = NULL;
    FuriPubSub* input_events = NULL;
    FuriPubSubSubscription* input_sub = NULL;
    FuriMessageQueue* q_local = NULL;

    __atomic_store_n(&s_input_cb_inflight, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&s_fb_cb_inflight, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&s_input_queue, (FuriMessageQueue*)NULL, __ATOMIC_RELEASE);

    buf = g_framebuffer;
    memset(g_framebuffer, 0x00, FB_SIZE);

    FlipperState* st = (FlipperState*)malloc(sizeof(FlipperState));
    if(!st) return -1;
    memset(st, 0, sizeof(FlipperState));
    g_state = st;

    bool fb_cb_added = false;

    do {
        st->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        if(!st->mutex) break;

        st->fb_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        if(!st->fb_mutex) break;

        memset(st->front_buffer, 0x00, FB_SIZE);

        q_local = furi_message_queue_alloc(32, sizeof(InputEvent));
        if(!q_local) break;
        __atomic_store_n(&s_input_queue, q_local, __ATOMIC_RELEASE);

        //(void)FX::begin(0, 0);

        gui = (Gui*)furi_record_open(RECORD_GUI);
        if(!gui) break;
        st->gui = gui;

        gui_add_framebuffer_callback(gui, framebuffer_commit_callback, st);
        fb_cb_added = true;

        canvas = gui_direct_draw_acquire(gui);
        if(!canvas) break;
        st->canvas = canvas;

        input_events = (FuriPubSub*)furi_record_open(RECORD_INPUT_EVENTS);
        if(!input_events) break;
        st->input_events = input_events;

        input_sub = furi_pubsub_subscribe(input_events, input_events_callback, st);
        if(!input_sub) break;
        st->input_sub = input_sub;

        a.begin(g_framebuffer, &st->input_state, st->mutex, &st->exit_requested);
        a.audio.begin();
        g_arduboy_audio_enabled = a.audio.enabled();

        g_audio_enabled_cb = audio_enabled_cb_impl;
        sound.~ArduboyTones();
        new(&sound) ArduboyTones(g_audio_enabled_cb);

        rand_seed = (uint16_t)((furi_hal_random_get() % 65534u) + 1u);

        game_setup();

        const uint32_t tick_hz = furi_kernel_get_tick_frequency();
        uint32_t period_ticks = (tick_hz + 15u) / 30u;
        if(period_ticks == 0) period_ticks = 1;

        uint32_t next_tick = furi_get_tick();

        while(!st->exit_requested) {
            uint32_t now = furi_get_tick();

            if((int32_t)(now - next_tick) < 0) {
                uint32_t dt_ticks = next_tick - now;
                uint32_t dt_ms = (dt_ticks * 1000u) / tick_hz;
                furi_delay_ms(dt_ms ? dt_ms : 1);
                continue;
            }

            if((int32_t)(now - next_tick) > (int32_t)(period_ticks * 2)) {
                next_tick = now;
            }
            next_tick += period_ticks;

            InputEvent ev;
            while(q_local && (furi_message_queue_get(q_local, &ev, 0) == FuriStatusOk)) {
                if(ev.key == InputKeyBack && ev.type == InputTypeLong) {
                    uint8_t gst = (uint8_t)state;
                    if(gst == 0) {
                        st->exit_requested = true;
                    }
                    break;
                }
                InputEvent tmp = ev;
                Arduboy2Base::FlipperInputCallback(&tmp, a.inputContext());
            }

            if(st->exit_requested) break;

            furi_mutex_acquire(st->mutex, FuriWaitForever);
            game_loop();
            furi_mutex_release(st->mutex);

            furi_mutex_acquire(st->fb_mutex, FuriWaitForever);
            memcpy(st->front_buffer, g_framebuffer, FB_SIZE);
            furi_mutex_release(st->fb_mutex);

            canvas_commit(canvas);
        }
    } while(false);

    st->shutting_down = true;

    sound.noTone();
    a.audio.off();
    furi_delay_ms(100);
    EEPROM.commit();

    __atomic_store_n(&s_input_queue, (FuriMessageQueue*)NULL, __ATOMIC_RELEASE);

    if(input_sub && input_events) {
        furi_pubsub_unsubscribe(input_events, input_sub);
        input_sub = NULL;
        st->input_sub = NULL;
    }

    wait_inflight_zero(&s_input_cb_inflight);

    if(input_events) {
        furi_record_close(RECORD_INPUT_EVENTS);
        input_events = NULL;
        st->input_events = NULL;
    }

    if(q_local) {
        furi_message_queue_free(q_local);
        q_local = NULL;
    }

    if(gui && fb_cb_added) {
        gui_remove_framebuffer_callback(gui, framebuffer_commit_callback, st);
        fb_cb_added = false;
    }

    wait_inflight_zero(&s_fb_cb_inflight);

    if(gui) {
        if(canvas) {
            gui_direct_draw_release(gui);
            canvas = NULL;
            st->canvas = NULL;
        }
        furi_record_close(RECORD_GUI);
        gui = NULL;
        st->gui = NULL;
    }

    sound.~ArduboyTones();
    new(&sound) ArduboyTones((bool (*)())0);
    g_audio_enabled_cb = NULL;

    if(st->fb_mutex) {
        furi_mutex_free(st->fb_mutex);
        st->fb_mutex = NULL;
    }

    if(st->mutex) {
        furi_mutex_free(st->mutex);
        st->mutex = NULL;
    }

    FlipperState* to_free = g_state;
    g_state = NULL;
    free(to_free);

    return 0;
}
