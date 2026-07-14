#include "../runtime.h"

#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/canvas.h>
#ifdef ARDULIB_USE_VIEW_PORT
#include <gui/canvas_i.h>
#endif
#include <input/input.h>

#include <stdlib.h>
#include <string.h>

#ifdef ARDULIB_USE_ATM
#include "../ATMlib.h"
#endif

extern Arduboy2Base arduboy;

constexpr size_t RuntimeWidth = 128u;
constexpr size_t RuntimeHeight = 64u;
constexpr size_t RuntimeBufferSize = (RuntimeWidth * RuntimeHeight) / 8u;

typedef struct {
    uint8_t screen_buffer[RuntimeBufferSize];

    Gui* gui;
    Canvas* canvas;
    FuriMutex* game_mutex;

#ifdef ARDULIB_USE_VIEW_PORT
    ViewPort* view_port;
#else
    FuriPubSub* input_events;
    FuriPubSubSubscription* input_sub;
#endif

    volatile uint8_t input_state;
    volatile uint8_t input_press_latch;
    volatile bool exit_requested;
    volatile bool input_cb_enabled;
    volatile uint32_t input_cb_inflight;
    volatile bool screen_inverted;
} ArduboyRuntimeState;

ArduboyRuntimeState rt_state;
bool rt_state_initialized = false;

void rt_wait_input_callbacks_idle(ArduboyRuntimeState* state) {
    if(!state) return;
    while(__atomic_load_n((uint32_t*)&state->input_cb_inflight, __ATOMIC_ACQUIRE) != 0) {
        furi_delay_ms(1);
    }
}

volatile bool g_arduboy_audio_enabled = false;
#ifdef ARDULIB_USE_TONES
FuriMessageQueue* g_arduboy_sound_queue = NULL;
FuriThread* g_arduboy_sound_thread = NULL;
volatile bool g_arduboy_sound_thread_running = false;
volatile bool g_arduboy_tones_playing = false;
volatile uint8_t g_arduboy_volume_mode = VOLUME_IN_TONE;
volatile bool g_arduboy_force_high = false;
volatile bool g_arduboy_force_norm = false;
#endif

uint8_t* buf = NULL;

bool arduboy_screen_inverted(void) {
    if(!rt_state_initialized) return false;
    return __atomic_load_n((bool*)&rt_state.screen_inverted, __ATOMIC_ACQUIRE);
}

void arduboy_screen_invert_toggle(void) {
    if(!rt_state_initialized) return;
    bool current = __atomic_load_n((bool*)&rt_state.screen_inverted, __ATOMIC_ACQUIRE);
    __atomic_store_n((bool*)&rt_state.screen_inverted, !current, __ATOMIC_RELEASE);
}

void arduboy_screen_invert(bool invert) {
    if(!rt_state_initialized) return;
    __atomic_store_n((bool*)&rt_state.screen_inverted, invert, __ATOMIC_RELEASE);
}

void rt_runtime_begin(
    uint8_t* screen_buffer,
    volatile uint8_t* input_state,
    volatile uint8_t* input_press_latch,
    FuriMutex* game_mutex,
    volatile bool* exit_requested) {
    UNUSED(screen_buffer);
    UNUSED(input_state);
    UNUSED(input_press_latch);
    UNUSED(game_mutex);
    UNUSED(exit_requested);

    // Инициализация Sprites для совместимости с играми
    Sprites::setArduboy(&arduboy);
}

uint16_t time_ms(void) {
    return (uint16_t)millis();
}

#ifdef ARDULIB_USE_VIEW_PORT
void rt_input_view_port_callback(InputEvent* event, void* context) {
    if(!event || !context) return;

    ArduboyRuntimeState* state = (ArduboyRuntimeState*)context;
    if(!__atomic_load_n((bool*)&state->input_cb_enabled, __ATOMIC_ACQUIRE)) return;

    (void)__atomic_fetch_add((uint32_t*)&state->input_cb_inflight, 1, __ATOMIC_ACQ_REL);

    if(__atomic_load_n((bool*)&state->input_cb_enabled, __ATOMIC_ACQUIRE)) {
        // Прямой вызов без InputContext - передаём указатель на arduboy
        Arduboy2Base::FlipperInputCallback(event, &arduboy);
    }

    (void)__atomic_fetch_sub((uint32_t*)&state->input_cb_inflight, 1, __ATOMIC_ACQ_REL);
}
#endif

#ifndef ARDULIB_USE_VIEW_PORT
void rt_input_events_callback(const void* value, void* ctx) {
    if(!value || !ctx) return;

    const InputEvent* event = (const InputEvent*)value;
    ArduboyRuntimeState* state = (ArduboyRuntimeState*)ctx;
    if(!__atomic_load_n((bool*)&state->input_cb_enabled, __ATOMIC_ACQUIRE)) return;

    (void)__atomic_fetch_add((uint32_t*)&state->input_cb_inflight, 1, __ATOMIC_RELAXED);

    Arduboy2Base::FlipperInputCallback(event, &arduboy);

    (void)__atomic_fetch_sub((uint32_t*)&state->input_cb_inflight, 1, __ATOMIC_RELAXED);
}
#endif

void rt_framebuffer_commit_callback(
    uint8_t* data,
    size_t size,
    CanvasOrientation orientation,
    void* context) {
    ArduboyRuntimeState* state = (ArduboyRuntimeState*)context;
    if(!state || !data) return;
    if(size < RuntimeBufferSize) return;
    (void)orientation;

    const uint8_t* src = state->screen_buffer;
    bool inverted = __atomic_load_n((bool*)&state->screen_inverted, __ATOMIC_ACQUIRE);

    for(size_t i = 0; i < RuntimeBufferSize; i++) {
        if(inverted) {
            data[i] = src[i];
        } else {
            data[i] = (uint8_t)(src[i] ^ 0xFF);
        }
    }
}

void rt_display(bool clear) {
    if(!rt_state_initialized) return;
    ArduboyRuntimeState* state = &rt_state;
#ifdef ARDULIB_USE_VIEW_PORT
    if(state->view_port) {
        view_port_update(state->view_port);
    }
    if(clear) {
        arduboy.clear();
    }
#else
    if(state->canvas) {
        canvas_commit(state->canvas);
    }
    if(clear) {
        arduboy.clear();
    }
#endif
}

void rt_step_frame(ArduboyRuntimeState* state) {
    if(!state || state->exit_requested) return;
    if(furi_mutex_acquire(state->game_mutex, 0) != FuriStatusOk) return;

    loop();

    furi_mutex_release(state->game_mutex);
}

#ifdef ARDULIB_USE_VIEW_PORT
void rt_view_port_draw_callback(Canvas* canvas, void* context) {
    ArduboyRuntimeState* state = (ArduboyRuntimeState*)context;
    if(!state || !canvas) return;

    uint8_t* data = u8g2_GetBufferPtr(&canvas->fb); //canvas_get_buffer
    if(!data) return;

    const uint8_t* src = state->screen_buffer;
    bool inverted = __atomic_load_n((bool*)&state->screen_inverted, __ATOMIC_ACQUIRE);

    for(size_t i = 0; i < RuntimeBufferSize; i++) {
        if(inverted) {
            data[i] = src[i];
        } else {
            data[i] = (uint8_t)(src[i] ^ 0xFF);
        }
    }
}
#endif


extern "C" int32_t arduboy_app(void* p) {
    UNUSED(p);

    ArduboyRuntimeState* state = &rt_state;
    memset(state, 0, sizeof(ArduboyRuntimeState));
    rt_state_initialized = true;

    state->game_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    state->input_cb_enabled = true;
    state->input_cb_inflight = 0;
    state->screen_inverted = false;

    if(!state->game_mutex) {
        if(state->game_mutex) furi_mutex_free(state->game_mutex);
        rt_state_initialized = false;
        return -1;
    }

    memset(state->screen_buffer, 0x00, RuntimeBufferSize);
    buf = state->screen_buffer;

    // Прямая инициализация arduboy с передачей всех необходимых указателей
    arduboy.begin(
        state->screen_buffer,
        &state->input_state,
        &state->input_press_latch,
        state->game_mutex,
        &state->exit_requested);
    rt_runtime_begin(
        state->screen_buffer,
        &state->input_state,
        &state->input_press_latch,
        state->game_mutex,
        &state->exit_requested);

    state->gui = (Gui*)furi_record_open(RECORD_GUI);
    if(!state->gui) {
        if(state->gui) furi_record_close(RECORD_GUI);
        furi_mutex_free(state->game_mutex);
        rt_state_initialized = false;
        buf = NULL;
        return -1;
    }

#ifdef ARDULIB_USE_VIEW_PORT
    state->view_port = view_port_alloc();
    if(!state->view_port) {
        if(state->gui) furi_record_close(RECORD_GUI);
        furi_mutex_free(state->game_mutex);
        rt_state_initialized = false;
        buf = NULL;
        return -1;
    }

    view_port_draw_callback_set(state->view_port, rt_view_port_draw_callback, state);
    view_port_input_callback_set(state->view_port, rt_input_view_port_callback, state);
    gui_add_view_port(state->gui, state->view_port, GuiLayerFullscreen);
#else
    gui_add_framebuffer_callback(state->gui, rt_framebuffer_commit_callback, state);
    state->canvas = gui_direct_draw_acquire(state->gui);

    state->input_events = (FuriPubSub*)furi_record_open(RECORD_INPUT_EVENTS);
    if(state->input_events) {
        state->input_sub =
            furi_pubsub_subscribe(state->input_events, rt_input_events_callback, state);
    }
#endif

#ifndef ARDULIB_USE_VIEW_PORT
    if(state->canvas) {
        canvas_clear(state->canvas);
        canvas_commit(state->canvas);
    }
#endif

    if(furi_mutex_acquire(state->game_mutex, FuriWaitForever) == FuriStatusOk) {
        setup();
        furi_mutex_release(state->game_mutex);
    }

    while(!state->exit_requested) {
        rt_step_frame(state);
        furi_delay_ms(1);
    }

    arduboy.audio.off();

    __atomic_store_n((bool*)&state->input_cb_enabled, false, __ATOMIC_RELEASE);

#ifdef ARDULIB_USE_VIEW_PORT
    if(state->view_port && state->gui) {
        view_port_enabled_set(state->view_port, false);
        gui_remove_view_port(state->gui, state->view_port);
        view_port_free(state->view_port);
        state->view_port = NULL;
    }
#else
    if(state->input_sub) {
        furi_pubsub_unsubscribe(state->input_events, state->input_sub);
        state->input_sub = NULL;
    }

    rt_wait_input_callbacks_idle(state);

    if(state->input_events) {
        furi_record_close(RECORD_INPUT_EVENTS);
        state->input_events = NULL;
    }

    if(state->canvas && state->gui) {
        gui_direct_draw_release(state->gui);
        state->canvas = NULL;
    }
#endif

    if(state->gui) {
#ifndef ARDULIB_USE_VIEW_PORT
        gui_remove_framebuffer_callback(state->gui, rt_framebuffer_commit_callback, state);
#endif
        furi_record_close(RECORD_GUI);
        state->gui = NULL;
    }

    rt_wait_input_callbacks_idle(state);

    if(state->game_mutex) {
        furi_mutex_free(state->game_mutex);
        state->game_mutex = NULL;
    }

    rt_state_initialized = false;
    buf = NULL;

    return 0;
}
