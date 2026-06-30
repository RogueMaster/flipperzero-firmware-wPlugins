#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#include "game_state.h"
#include "src/util/list.h"
#include "src/scene/scene_setup.h"
#include "src/util/helpers.h"

static List* game_logic;
static ListItem* current_state;
static FuriMutex* update_mutex;

static void gui_input_events_callback(const void* value, void* ctx) {
    furi_mutex_acquire(update_mutex, FuriWaitForever);
    GameState* instance = ctx;
    const InputEvent* event = value;

    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        FURI_LOG_W("INPUT", "EXIT");
        instance->exit = true;
    }

    if(current_state) {
        ((GameLogic*)current_state->data)->input(instance, event->key, event->type);
    }
    furi_mutex_release(update_mutex);
}

static GameState* prepare() {
    game_logic = list_make();
    update_mutex = (FuriMutex*)furi_mutex_alloc(FuriMutexTypeNormal);

    //Add scenes to the logic list
    list_push_back(&main_screen, game_logic);
    list_push_back(&intro_screen, game_logic);
    list_push_back(&play_screen, game_logic);
    list_push_back(&solve_screen, game_logic);
    list_push_back(&falling_screen, game_logic);
    list_push_back(&result_screen, game_logic);

    current_state = game_logic->head;

    GameState* instance = malloc(sizeof(GameState));
    ((GameLogic*)current_state->data)->start(instance);

    instance->hand = list_make();
    instance->deck = list_make();
    instance->waste = list_make();
    for(int i = 0; i < 7; i++) {
        if(i < 4) {
            instance->foundation[i] = list_make();
        }
        instance->tableau[i] = list_make();
    }

    instance->animated_card.position = VECTOR_ZERO;
    instance->animated_card.velocity = VECTOR_ZERO;

    instance->buffer = buffer_create(SCREEN_WIDTH, SCREEN_HEIGHT, false);
    instance->input = furi_record_open(RECORD_INPUT_EVENTS);
    instance->gui = furi_record_open(RECORD_GUI);
    instance->canvas = gui_direct_draw_acquire(instance->gui);
    instance->notification_app = (NotificationApp*)furi_record_open(RECORD_NOTIFICATION);
    notification_message_block(instance->notification_app, &sequence_display_backlight_enforce_on);

    instance->input_subscription =
        furi_pubsub_subscribe(instance->input, gui_input_events_callback, instance);

    return instance;
}

static void cleanup(GameState* instance) {
    furi_pubsub_unsubscribe(instance->input, instance->input_subscription);
    notification_message_block(
        instance->notification_app, &sequence_display_backlight_enforce_auto);

    list_free(instance->hand);
    list_free(instance->deck);
    list_free(instance->waste);
    for(int i = 0; i < 7; i++) {
        if(i < 4) {
            list_free(instance->foundation[i]);
        }
        list_free(instance->tableau[i]);
    }

    furi_mutex_free(update_mutex);
    instance->canvas = NULL;
    gui_direct_draw_release(instance->gui);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_INPUT_EVENTS);
    furi_record_close(RECORD_NOTIFICATION);

    list_clear(game_logic);
    free(game_logic);
    buffer_release(instance->buffer);
    free(instance);
}

static void next_scene(GameState* instance) {
    FURI_LOG_W("SCENE", "Next scene");
    current_state = current_state->next;
    if(current_state == NULL) {
        current_state = game_logic->head;
    }
    ((GameLogic*)current_state->data)->start(instance);
}

static void prev_scene(GameState* instance) {
    FURI_LOG_W("SCENE", "Prev scene");
    current_state = game_logic->head;
    if(current_state->prev == NULL) {
        instance->exit = true;
        return;
    }
    ((GameLogic*)current_state->data)->start(instance);
}

static void direct_draw_run(GameState* instance) {
    if(!check_pointer(instance)) return;

    size_t currFrameTime;
    size_t lastFrameTime = curr_time();
    instance->lateRender = false;

    furi_thread_set_current_priority(FuriThreadPriorityIdle);
    do {
        FuriStatus status = furi_mutex_acquire(update_mutex, 20);
        if(!status) continue;

        GameLogic* curr_state = (GameLogic*)current_state->data;
        currFrameTime = curr_time();
        instance->delta_time = (currFrameTime - lastFrameTime) / 64000000.0f;
        lastFrameTime = currFrameTime;

        check_pointer(curr_state);
        curr_state->update(instance);
        if(instance->scene_switch == 1) {
            next_scene(instance);
        } else if(instance->scene_switch == 2) {
            prev_scene(instance);
        }
        check_pointer(curr_state);
        check_pointer(instance);
        check_pointer(instance->canvas);
        check_pointer(instance->buffer);
        instance->scene_switch = 0;
        if(curr_state && instance->isDirty && instance->canvas && instance->buffer) {
            canvas_reset(instance->canvas);

            if(instance->lateRender) {
                buffer_swap_back(instance->buffer);
                buffer_render(instance->buffer, instance->canvas);
                curr_state->render(instance);
            } else {
                buffer_swap_back(instance->buffer);
                curr_state->render(instance);
                buffer_render(instance->buffer, instance->canvas);
            }
            canvas_commit(instance->canvas);

            if(instance->clearBuffer) buffer_clear(instance->buffer);

            instance->clearBuffer = true;
            instance->lateRender = false;
            instance->isDirty = false;
        }
        furi_mutex_release(update_mutex);
        furi_thread_yield();
    } while(!instance->exit);
}

int32_t solitaire_app(void* p) {
    UNUSED(p);
    int32_t return_code = 0;
    GameState* instance = prepare();

    direct_draw_run(instance);

    cleanup(instance);
    return return_code;
}
