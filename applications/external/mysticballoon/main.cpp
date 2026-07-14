/*
  Mystic Balloon          http://www.team-arg.org/mybl-manual.html
  Arduboy version 1.7.2   http://www.team-arg.org/mybl-downloads.html
  MADE by TEAM a.r.g.     http://www.team-arg.org/more-about.html
  Game License: MIT       https://opensource.org/licenses/MIT
  2016-2018 - GAVENO - CastPixel - JO3RI - Martian220
*/

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>

#define COLOUR_WHITE 1
#define COLOUR_BLACK 0
#define TONES_END    0x8000

#define TARGET_FRAMERATE 50
#define GAME_ID          34

#ifndef HOLD_TO_EXIT_FRAMES
#define HOLD_TO_EXIT_FRAMES 30
#endif

#include "lib/Arduboy2.h"
#include "lib/Arduino.h"
#include "game/globals.h"
#include "game/menu.h"
#include "game/game.h"
#include "game/inputs.h"
#include "game/player.h"
#include "game/enemies.h"
#include "game/elements.h"
#include "game/levels.h"

using FunctionPointer = void (*)();
static FunctionPointer mainGameLoop[] = {
    stateMenuIntro,
    stateMenuMain,
    stateMenuHelp,
    stateMenuPlaySelect,
    stateMenuInfo,
    stateMenuSoundfx,
    stateGameNextLevel,
    stateGamePlaying,
    stateGamePause,
    stateGameOver,
    stateMenuPlayContinue,
    stateMenuPlayNew,
};

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64
#define BUFFER_SIZE    (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)

typedef struct {
    uint8_t screen_buffer[BUFFER_SIZE];
    uint8_t front_buffer[BUFFER_SIZE];

    Gui* gui;
    Canvas* canvas;
    FuriTimer* timer;
    FuriMutex* fb_mutex;
    FuriMutex* game_mutex;
    FuriPubSub* input_events;
    FuriPubSubSubscription* input_sub;
    volatile uint8_t input_state;
    volatile bool exit_requested;
    volatile bool in_frame;
    volatile bool invert_frame;
} FlipperState;

static FlipperState* g_state = NULL;
static volatile uint32_t g_input_cb_inflight = 0;
static volatile uint8_t g_input_cb_enabled = 0;

static void wait_input_callbacks_idle() {
    while(__atomic_load_n((uint32_t*)&g_input_cb_inflight, __ATOMIC_ACQUIRE) != 0) {
        furi_delay_ms(1);
    }
}
static void framebuffer_commit_callback(
    uint8_t* data,
    size_t size,
    CanvasOrientation orientation,
    void* context) {
    FlipperState* state = (FlipperState*)context;
    if(!state || !data) return;
    if(size < BUFFER_SIZE) return;
    (void)orientation;

    if(furi_mutex_acquire(state->fb_mutex, 0) != FuriStatusOk) return;

    const uint8_t* src = state->front_buffer;
    for(size_t i = 0; i < BUFFER_SIZE; i++) {
        data[i] = (uint8_t)(src[i] ^ 0xFF);
    }

    furi_mutex_release(state->fb_mutex);
}

static void input_events_callback(const void* value, void* ctx) {
    if(!value || !ctx) return;
    if(__atomic_load_n((uint8_t*)&g_input_cb_enabled, __ATOMIC_ACQUIRE) == 0) return;

    (void)__atomic_fetch_add((uint32_t*)&g_input_cb_inflight, 1, __ATOMIC_ACQ_REL);

    if(__atomic_load_n((uint8_t*)&g_input_cb_enabled, __ATOMIC_ACQUIRE) != 0) {
        const InputEvent* event = (const InputEvent*)value;
        Arduboy2Base::InputContext* input_ctx = (Arduboy2Base::InputContext*)ctx;
        Arduboy2Base::FlipperInputCallback(event, input_ctx);
    }

    (void)__atomic_fetch_sub((uint32_t*)&g_input_cb_inflight, 1, __ATOMIC_ACQ_REL);
}

static void game_setup() {
    arduboy.boot();
    arduboy.audio.begin();
    arduboy.setFrameRate(TARGET_FRAMERATE);
    loadSetEEPROM();
    arduboy.pollButtons();
    arduboy.clear();
    mainGameLoop[gameState]();
    arduboy.display();
}

static uint8_t exit_hold_frames = 0;
static bool menu_exit_requires_release = false;

static bool has_visible_enemy_on_screen() {
    const int16_t view_left = cam.pos.x;
    const int16_t view_top = cam.pos.y;
    const int16_t view_right = cam.pos.x + DISPLAY_WIDTH - 1;
    const int16_t view_bottom = cam.pos.y + DISPLAY_HEIGHT - 1;

    for(uint8_t i = 0; i < MAX_PER_TYPE; ++i) {
        if(!walkers[i].active || walkers[i].HP <= 0) continue;

        const int16_t enemy_left = walkers[i].pos.x;
        const int16_t enemy_top = walkers[i].pos.y;
        const int16_t enemy_right = walkers[i].pos.x + 7;
        const int16_t enemy_bottom = walkers[i].pos.y + 7;

        const bool intersects =
            !(enemy_left > view_right || enemy_right < view_left || enemy_top > view_bottom ||
              enemy_bottom < view_top);
        if(intersects) {
            return true;
        }
    }

    return false;
}

static void game_loop_tick() {
    arduboy.pollButtons();
    if(!(arduboy.nextFrame())) return;

    const bool menu_state = (gameState < STATE_GAME_NEXT_LEVEL);
    const bool in_game_state = !menu_state;
    const bool a_pressed = arduboy.pressed(A_BUTTON);

    bool can_hold_to_exit = menu_state;
    bool hold_action_exits_app = menu_state;

    if(gameState == STATE_GAME_PLAYING) {
        const bool enemy_visible = has_visible_enemy_on_screen();
        if(kid.isSucking) {
            can_hold_to_exit = false;
            hold_action_exits_app = false;
        } else if(!enemy_visible) {
            can_hold_to_exit = true;
            hold_action_exits_app = false;
        } else {
            can_hold_to_exit = false;
            hold_action_exits_app = false;
        }
    } else if(in_game_state) {
        can_hold_to_exit = true;
        hold_action_exits_app = false;
    }

    if(menu_state && menu_exit_requires_release) {
        if(!a_pressed) {
            menu_exit_requires_release = false;
        } else {
            can_hold_to_exit = false;
            hold_action_exits_app = true;
        }
    }

    if(can_hold_to_exit) {
        if(a_pressed) {
            if(exit_hold_frames < HOLD_TO_EXIT_FRAMES) {
                exit_hold_frames++;
            } else {
                EEPROM.commit();
                if(hold_action_exits_app) {
                    if(g_state) g_state->exit_requested = true;
                } else {
                    gameState = STATE_MENU_MAIN;
                    menu_exit_requires_release = true;
                }
                exit_hold_frames = 0;
            }
        } else {
            exit_hold_frames = 0;
        }
    } else {
        exit_hold_frames = 0;
    }

    if(gameState < STATE_GAME_NEXT_LEVEL && arduboy.everyXFrames(10)) {
        sparkleFrames = (sparkleFrames + 1) % 5;
    }

    arduboy.clear();
    mainGameLoop[gameState]();
    arduboy.display();
}

static void timer_callback(void* ctx) {
    FlipperState* state = (FlipperState*)ctx;
    if(!state) return;

    if(state->in_frame) return;
    state->in_frame = true;
    if(furi_mutex_acquire(state->game_mutex, 0) != FuriStatusOk) {
        state->in_frame = false;
        return;
    }

    game_loop_tick();
    state->invert_frame = false;
    furi_mutex_acquire(state->fb_mutex, FuriWaitForever);
    memcpy(state->front_buffer, state->screen_buffer, BUFFER_SIZE);
    furi_mutex_release(state->fb_mutex);
    canvas_commit(state->canvas);
    furi_mutex_release(state->game_mutex);
    state->in_frame = false;
}

extern "C" int32_t mybl_app(void* p) {
    UNUSED(p);

    g_state = (FlipperState*)malloc(sizeof(FlipperState));
    if(!g_state) return -1;
    memset(g_state, 0, sizeof(FlipperState));

    g_state->fb_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    g_state->game_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!g_state->fb_mutex || !g_state->game_mutex) {
        if(g_state->fb_mutex) furi_mutex_free(g_state->fb_mutex);
        if(g_state->game_mutex) furi_mutex_free(g_state->game_mutex);
        free(g_state);
        g_state = NULL;
        return -1;
    }

    g_state->exit_requested = false;
    g_state->invert_frame = false;
    __atomic_store_n((uint32_t*)&g_input_cb_inflight, 0, __ATOMIC_RELEASE);
    __atomic_store_n((uint8_t*)&g_input_cb_enabled, 1, __ATOMIC_RELEASE);

    memset(g_state->screen_buffer, 0x00, BUFFER_SIZE);
    memset(g_state->front_buffer, 0x00, BUFFER_SIZE);

    arduboy.begin(
        g_state->screen_buffer,
        &g_state->input_state,
        g_state->game_mutex,
        &g_state->exit_requested);
    Sprites::setArduboy(&arduboy);
    g_state->gui = (Gui*)furi_record_open(RECORD_GUI);
    gui_add_framebuffer_callback(g_state->gui, framebuffer_commit_callback, g_state);
    g_state->canvas = gui_direct_draw_acquire(g_state->gui);
    g_state->input_events = (FuriPubSub*)furi_record_open(RECORD_INPUT_EVENTS);
    g_state->input_sub = furi_pubsub_subscribe(
        g_state->input_events, input_events_callback, arduboy.inputContext());

    furi_mutex_acquire(g_state->game_mutex, FuriWaitForever);
    game_setup();
    furi_mutex_acquire(g_state->fb_mutex, FuriWaitForever);
    memcpy(g_state->front_buffer, g_state->screen_buffer, BUFFER_SIZE);
    furi_mutex_release(g_state->fb_mutex);
    canvas_commit(g_state->canvas);
    furi_mutex_release(g_state->game_mutex);
    g_state->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, g_state);
    const uint32_t tick_hz = furi_kernel_get_tick_frequency();
    uint32_t period = (tick_hz + (TARGET_FRAMERATE / 2)) / TARGET_FRAMERATE;
    if(period == 0) period = 1;
    furi_timer_start(g_state->timer, period);

    while(!g_state->exit_requested) {
        furi_delay_ms(50);
    }

    arduboy_tone_sound_system_deinit();

    if(g_state->timer) {
        furi_timer_stop(g_state->timer);
        furi_timer_free(g_state->timer);
        g_state->timer = NULL;
    }

    if(g_state->input_sub) {
        __atomic_store_n((uint8_t*)&g_input_cb_enabled, 0, __ATOMIC_RELEASE);
        furi_pubsub_unsubscribe(g_state->input_events, g_state->input_sub);
        g_state->input_sub = NULL;
    }

    wait_input_callbacks_idle();

    Arduboy2Base::InputContext* input_ctx = arduboy.inputContext();
    input_ctx->input_state = nullptr;
    input_ctx->runtime = nullptr;

    if(g_state->input_events) {
        furi_record_close(RECORD_INPUT_EVENTS);
        g_state->input_events = NULL;
    }

    if(g_state->gui) {
        gui_direct_draw_release(g_state->gui);
        gui_remove_framebuffer_callback(g_state->gui, framebuffer_commit_callback, g_state);
        furi_record_close(RECORD_GUI);
        g_state->gui = NULL;
        g_state->canvas = NULL;
    }

    if(g_state->fb_mutex) {
        furi_mutex_free(g_state->fb_mutex);
        g_state->fb_mutex = NULL;
    }
    if(g_state->game_mutex) {
        furi_mutex_free(g_state->game_mutex);
        g_state->game_mutex = NULL;
    }

    free(g_state);
    g_state = NULL;

    return 0;
}
