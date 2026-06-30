#include "result_screen.h"
#include "../../game_state.h"
#include "../util/helpers.h"
#include <dolphin/dolphin.h>
#include <notification/notification_messages.h>

static int hours, minutes, seconds;
static bool isStarted = false;
static char timeString[24];
static const NotificationSequence sequence_cheer = {
    &message_note_c4,
    &message_delay_100,
    &message_note_e4,
    &message_delay_100,
    &message_note_g4,
    &message_delay_100,
    &message_note_a4,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

void start_result_screen(void* data) {
    GameState* state = (GameState*)data;
    dolphin_deed(DolphinDeedPluginGameWin);
    size_t diff = (state->game_end - state->game_start) / furi_kernel_get_tick_frequency();
    hours = (int)(diff / 3600);
    minutes = (int)(diff % 3600) / 60;
    seconds = (int)(diff % 60);
    state->lateRender = true;
    state->isDirty = true;
    state->clearBuffer = false;
    isStarted = false;
    notification_message(state->notification_app, (const NotificationSequence*)&sequence_cheer);
}

void render_result_screen(void* data) {
    GameState* state = (GameState*)data;

    canvas_set_color(state->canvas, ColorWhite);
    canvas_draw_box(state->canvas, 22, 14, 85, 30);
    canvas_set_color(state->canvas, ColorBlack);
    canvas_draw_frame(state->canvas, 21, 13, 87, 32);

    canvas_set_font(state->canvas, FontPrimary);
    canvas_draw_str_aligned(state->canvas, 64, 15, AlignCenter, AlignTop, "Congratulations!");
    canvas_set_font(state->canvas, FontSecondary);
    canvas_draw_str_aligned(state->canvas, 64, 26, AlignCenter, AlignTop, "Solve time:");

    if(hours > 0)
        snprintf(timeString, sizeof(timeString), "%02d:%02d:%02d", hours, minutes, seconds);
    else
        snprintf(timeString, sizeof(timeString), "%02d:%02d", minutes, seconds);
    canvas_set_font(state->canvas, FontSecondary);
    canvas_draw_str_aligned(state->canvas, 64, 35, AlignCenter, AlignTop, timeString);
}

void update_result_screen(void* data) {
    GameState* state = (GameState*)data;
    state->clearBuffer = false;
    state->lateRender = true;
    if(!isStarted) {
        state->isDirty = true;
        isStarted = true;
    }
}

void input_result_screen(void* data, InputKey key, InputType type) {
    GameState* state = (GameState*)data;
    if(key == InputKeyOk && type == InputTypePress) {
        state->scene_switch = 1;
    }
}
