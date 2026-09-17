#include <furi.h>

#include "threads.h"
#include "constants.h"
#include "game_structs.h"
#include "settings_management.h"
#include "state_management.h"
#include "gui/utils.h"

void main_thread(struct ApplicationContext *context) {
    furi_thread_start(context->secondary_thread);
    FURI_LOG_D(LOG_TAG, "Main thread started");

    start_gui_and_block(context); // blocks until GUI exits

    struct ThreadsMessage threads_message = {.type = SAVE_AND_EXIT};
    furi_message_queue_put(context->threads_message_queue, &threads_message, FuriWaitForever);
    furi_thread_join(context->secondary_thread);
    FURI_LOG_D(LOG_TAG, "Ciao!");
}

/* Turn logic event flags into sound + vibration cues. */
static void play_for_flags(struct GameState *gs, GameEventFlags f) {
    if(f & EVT_EVOLVED) { play_level_up(gs); vibrate_long(gs); }
    if(f & EVT_DIED) { play_starvation(gs); vibrate_long(gs); }
    else if(f & EVT_SICK) { play_ambulance(gs); vibrate_long(gs); }
    else if(f & EVT_STARVING) { play_starvation(gs); vibrate_long(gs); }
    if(f & (EVT_FED | EVT_PLAYED | EVT_CLEANED | EVT_HEALED)) { play_action(gs); vibrate_short(gs); }
    if(f & EVT_CALL) { play_action(gs); vibrate_short(gs); }
    if(f & EVT_EXPED_RETURN) { play_level_up(gs); vibrate_long(gs); }
}

int32_t secondary_thread(void *ctx) {
    struct ApplicationContext *context = (struct ApplicationContext *)ctx;
    FURI_LOG_D(LOG_TAG, "Secondary thread started");

    init_settings(context->game_state);
    GameEventFlags offline = init_state(context->game_state);
    switch_to_main_scene(context);
    // "While you were away": if the pet needs urgent attention now, call out.
    if(offline & (EVT_DIED | EVT_SICK | EVT_STARVING)) {
        play_for_flags(context->game_state, offline);
    }

    struct ThreadsMessage message;
    while(true) {
        FuriStatus status = furi_message_queue_get(
            context->threads_message_queue, &message, BACKGROUND_ACTIVITY_TICKS);
        if(status == FuriStatusOk) {
            switch(message.type) {
                case SAVE_AND_EXIT:
                    persist_settings(context->game_state);
                    persist_state(context->game_state);
                    return 0;
                case RESET_STATE:
                    reset_state(context->game_state);
                    context->game_state->next_animation_index = 0;
                    go_back_to_main_scene(context);
                    break;
                case PROCESS_FORAGE: {
                    GameEventFlags f = do_forage(context->game_state);
                    if(f & EVT_CAUGHT) {
                        struct GameState *gs = context->game_state;
                        switch(gs->last_catch.category) {
                            case CATCH_PREY:     play_action(gs); vibrate_short(gs); break;
                            case CATCH_TREASURE: play_level_up(gs); vibrate_short(gs); break;
                            default:             play_level_up(gs); vibrate_long(gs); break; // egg
                        }
                    } else {
                        vibrate_short(context->game_state); // on cooldown
                    }
                    context->game_state->next_animation_index = 0;
                    send_tick_to_scene(context);
                    break;
                }
                case PROCESS_EXPEDITION:
                    do_expedition(context->game_state, (uint16_t)message.arg);
                    context->game_state->next_animation_index = 0;
                    send_tick_to_scene(context);
                    break;
                case PROCESS_HATCH_HEIR: {
                    do_hatch_heir(context->game_state);
                    play_level_up(context->game_state);
                    vibrate_long(context->game_state);
                    context->game_state->next_animation_index = 0;
                    send_tick_to_scene(context);
                    break;
                }
                default: {
                    GameEventFlags f = do_action(context->game_state, message.type);
                    play_for_flags(context->game_state, f);
                    context->game_state->next_animation_index = 0;
                    send_tick_to_scene(context);
                    break;
                }
            }
        } else if(status == FuriStatusErrorTimeout) {
            GameEventFlags f = tick_state(context->game_state);
            play_for_flags(context->game_state, f);
            if(f) context->game_state->next_animation_index = 0;
            else context->game_state->next_animation_index++;
            send_tick_to_scene(context);
        } else {
            furi_crash("Unexpected status in game event queue");
        }
    }
    return 0;
}
