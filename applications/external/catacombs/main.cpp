#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>

#include <stdlib.h>
#include <string.h>

#include "lib/flipper.h"
#include "lib/EEPROM.h"
#include "game/Game.h"
#include "game/Platform.h"

#define TARGET_FRAMERATE 30
#define HOLD_TIME_MS     300

FlipperState* g_state = NULL;

static volatile uint32_t s_input_cb_inflight = 0;
static volatile uint32_t s_fb_cb_inflight = 0;
static volatile uint8_t s_back_pressed = 0;

static inline void wait_inflight_zero(volatile uint32_t* counter) {
    while(__atomic_load_n(counter, __ATOMIC_ACQUIRE) != 0) {
        furi_delay_ms(1);
    }
}

inline bool audio_enable() {
    return !furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode);
}

static void framebuffer_commit_callback(
    uint8_t* data,
    size_t size,
    CanvasOrientation orientation,
    void* context) {
    __atomic_fetch_add(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);

    FlipperState* state = (FlipperState*)context;
    if(!state || !data || size < BUFFER_SIZE) {
        __atomic_fetch_sub(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);
        return;
    }
    (void)orientation;

    if(furi_mutex_acquire(state->fb_mutex, 0) != FuriStatusOk) {
        __atomic_fetch_sub(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);
        return;
    }

    const uint8_t* src = state->front_buffer;
    for(size_t i = 0; i < BUFFER_SIZE; i++) {
        data[i] = (uint8_t)(src[i] ^ 0xFF);
    }

    furi_mutex_release(state->fb_mutex);

    __atomic_fetch_sub(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);
}

static void input_events_callback(const void* value, void* ctx) {
    if(!value || !ctx) return;

    __atomic_fetch_add(&s_input_cb_inflight, 1, __ATOMIC_RELAXED);

    FlipperState* state = (FlipperState*)ctx;
    const InputEvent* event = (const InputEvent*)value;

    uint8_t bit = 0;
    switch(event->key) {
    case InputKeyUp:
        bit = INPUT_UP;
        break;
    case InputKeyDown:
        bit = INPUT_DOWN;
        break;
    case InputKeyLeft:
        bit = INPUT_LEFT;
        break;
    case InputKeyRight:
        bit = INPUT_RIGHT;
        break;
    case InputKeyOk:
        bit = INPUT_B;
        break;
    case InputKeyBack:
        if((event->type == InputTypePress) || (event->type == InputTypeRepeat)) {
            (void)__atomic_store_n(&s_back_pressed, 1, __ATOMIC_RELAXED);
        } else if(event->type == InputTypeRelease) {
            (void)__atomic_store_n(&s_back_pressed, 0, __ATOMIC_RELAXED);
        }
        break;
    default:
        break;
    }

    if(state && bit) {
        if((event->type == InputTypePress) || (event->type == InputTypeRepeat)) {
            (void)__atomic_fetch_or((uint8_t*)&state->input_state, bit, __ATOMIC_RELAXED);
        } else if(event->type == InputTypeRelease) {
            (void)__atomic_fetch_and(
                (uint8_t*)&state->input_state, (uint8_t)~bit, __ATOMIC_RELAXED);
        }
    }

    __atomic_fetch_sub(&s_input_cb_inflight, 1, __ATOMIC_RELAXED);
}

extern "C" int32_t arduboy3d_app(void* p) {
    UNUSED(p);

    Gui* gui = NULL;
    Canvas* canvas = NULL;
    FuriPubSub* input_events = NULL;
    FuriPubSubSubscription* input_sub = NULL;

    FlipperState* st = (FlipperState*)malloc(sizeof(FlipperState));
    if(!st) return -1;
    memset(st, 0, sizeof(FlipperState));
    g_state = st;

    do {
        st->fb_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        if(!st->fb_mutex) break;

        memset(st->back_buffer, 0x00, BUFFER_SIZE);
        memset(st->front_buffer, 0x00, BUFFER_SIZE);

        EEPROM.begin();
        furi_delay_ms(50);
        Platform::SetAudioEnabled(audio_enable());
        Game::menu.ReadSave();

        gui = (Gui*)furi_record_open(RECORD_GUI);
        if(!gui) break;
        st->gui = gui;

        gui_add_framebuffer_callback(gui, framebuffer_commit_callback, st);

        canvas = gui_direct_draw_acquire(gui);
        if(!canvas) break;
        st->canvas = canvas;

        input_events = (FuriPubSub*)furi_record_open(RECORD_INPUT_EVENTS);
        if(!input_events) break;
        st->input_events = input_events;

        input_sub = furi_pubsub_subscribe(input_events, input_events_callback, st);
        if(!input_sub) break;
        st->input_sub = input_sub;

        const uint32_t tick_hz = furi_kernel_get_tick_frequency();
        uint32_t period_ticks = (tick_hz + (TARGET_FRAMERATE / 2)) / TARGET_FRAMERATE;
        if(period_ticks == 0) period_ticks = 1;
        const uint32_t hold_ticks = (uint32_t)((HOLD_TIME_MS * tick_hz + 999u) / 1000u);

        uint32_t next_tick = furi_get_tick();

        bool back_was_pressed = false;
        bool back_hold_fired = false;
        uint32_t back_press_tick = 0;

        while(!st->exit_requested) {
            uint32_t now = furi_get_tick();

            // frame pacing
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

            const bool back_pressed = (__atomic_load_n(&s_back_pressed, __ATOMIC_RELAXED) != 0);

            // BACK hold logic
            if(!back_pressed) {
                back_was_pressed = false;
                back_hold_fired = false;
            } else {
                if(!back_was_pressed) {
                    back_was_pressed = true;
                    back_press_tick = now;
                    back_hold_fired = false;
                }

                if(!back_hold_fired && ((uint32_t)(now - back_press_tick) >= hold_ticks)) {
                    back_hold_fired = true;
                    if(Game::InMenu())
                        st->exit_requested = true;
                    else
                        Game::GoToMenu();
                }
            }

            if(st->exit_requested) break;

            Game::Tick();
            Game::Draw();

            // swap for framebuffer callback
            furi_mutex_acquire(st->fb_mutex, FuriWaitForever);
            memcpy(st->front_buffer, st->back_buffer, BUFFER_SIZE);
            furi_mutex_release(st->fb_mutex);

            canvas_commit(canvas);
        }
    } while(false);

    Game::menu.WriteSave();

    if(input_sub && input_events) {
        furi_pubsub_unsubscribe(input_events, input_sub);
        input_sub = NULL;
    }
    st->input_sub = NULL;

    wait_inflight_zero(&s_input_cb_inflight);
    (void)__atomic_store_n(&s_back_pressed, 0, __ATOMIC_RELAXED);

    if(input_events) {
        furi_record_close(RECORD_INPUT_EVENTS);
        input_events = NULL;
    }
    st->input_events = NULL;

    if(gui) {
        gui_remove_framebuffer_callback(gui, framebuffer_commit_callback, st);
    }

    wait_inflight_zero(&s_fb_cb_inflight);

    if(gui) {
        if(canvas) {
            gui_direct_draw_release(gui);
            canvas = NULL;
        }
        furi_record_close(RECORD_GUI);
        gui = NULL;
    }
    st->gui = NULL;
    st->canvas = NULL;

    if(st->fb_mutex) {
        furi_mutex_free(st->fb_mutex);
        st->fb_mutex = NULL;
    }

    Platform::SetAudioEnabled(false);

    free(st);
    g_state = NULL;

    return 0;
}
